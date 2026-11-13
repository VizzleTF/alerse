// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/alerse_sync_sk.h"

#include <algorithm>
#include <set>

#include "base/containers/span.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "third_party/boringssl/src/include/openssl/aead.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/hkdf.h"

namespace syncer {

namespace {

constexpr std::string_view kIdInfo = "alerse-sk-id";
constexpr std::string_view kMixInfo = "alerse-sk-mix";
constexpr std::string_view kMasterInfo = "alerse-sk-v1";
constexpr std::string_view kAuthInfo = "alerse-sk-auth";
constexpr std::string_view kWrapInfo = "alerse-sk-wrap";
constexpr std::string_view kWrapContext = "alerse-sk-v1";
constexpr uint8_t kWrapVersion = 0x01;
constexpr size_t kNonceSize = 12;
constexpr size_t kSaltSize = 32;
// The least cost a server may ask for: today's default. It stays where it is
// when the default rises, so that older accounts still open.
constexpr SyncKdf kMinKdf{.n = 1u << 17, .r = 8, .p = 1};

// Long passwords people pick often, from public breach lists. Shorter ones
// are refused for their length.
constexpr std::string_view kCommonPasswords[] = {
    "123456789012345",     "1234567890123456",    "12345678901234567890",
    "qwertyuiopasdfg",     "qwertyuiopasdfgh",    "qwertyuiopasdfghjkl",
    "qwertyuiopasdfghjklzxcvbnm", "1q2w3e4r5t6y7u8i9o0p",
    "1qaz2wsx3edc4rfv",    "1qaz2wsx3edc4rfv5tgb", "zaq12wsxcde34rfv",
    "passwordpassword",    "password123456789",   "password1234567",
    "passwordpassword1",   "iloveyouiloveyou",    "qwertyqwertyqwerty",
    "abcdefghijklmnop",    "abcdefghijklmnopqrstuvwxyz",
    "abc123abc123abc123",  "letmeinletmein1",     "welcome123456789",
    "administrator123",    "correcthorsebatterystaple",
    "correct horse battery staple", "thequickbrownfox",
    "thequickbrownfoxjumpsoverthelazydog", "trustno1trustno1",
    "football123456789",   "baseball123456789",   "superman123456789",
    "princess123456789",   "dragon1234567890",    "monkey1234567890",
    "sunshine123456789",   "changemechangeme",    "changeme123456789",
    "mypasswordismypassword", "thisismypassword",  "thisisapassword",
    "this is my password", "123qweasdzxc123",     "q1w2e3r4t5y6u7i8",
    "asdfghjklqwertyuiop", "zxcvbnmasdfghjkl",    "0987654321098765",
    "987654321987654321",  "111111111111111",     "000000000000000",
};

size_t CountCharacters(std::string_view text) {
  return std::ranges::count_if(
      text, [](char c) { return (static_cast<uint8_t>(c) & 0xC0) != 0x80; });
}

// "abcabcabc", "aaaa", "12341234", "qweqwe": a short pattern repeated.
bool IsRepetitive(std::string_view text) {
  const std::set<char> distinct(text.begin(), text.end());
  if (distinct.size() < 5) {
    return true;
  }
  // Each character one more or one less than the one before it.
  int steps = 0;
  for (size_t i = 1; i < text.size(); ++i) {
    const int step = text[i] - text[i - 1];
    if (step == 1 || step == -1) {
      ++steps;
    }
  }
  return steps + 1 >= static_cast<int>(text.size()) * 3 / 4;
}

std::array<uint8_t, 32> Hkdf(base::span<const uint8_t> secret,
                             std::string_view info) {
  std::array<uint8_t, 32> out;
  CHECK(HKDF(out.data(), out.size(), EVP_sha256(), secret.data(),
             secret.size(), nullptr, 0,
             reinterpret_cast<const uint8_t*>(info.data()), info.size()));
  return out;
}

std::string AdditionalData(std::string_view id) {
  return base::StrCat({kWrapContext, id});
}

void InitWrapCipher(EVP_AEAD_CTX* ctx, const SyncAccountKeys& keys) {
  CHECK(EVP_AEAD_CTX_init(ctx, EVP_aead_aes_256_gcm(), keys.wrap.data(),
                          keys.wrap.size(), EVP_AEAD_DEFAULT_TAG_LENGTH,
                          nullptr));
}

}  // namespace

std::string SyncAccountId(const SyncChainSecret& sk) {
  return base::HexEncodeLower(Hkdf(sk, kIdInfo));
}

bool IsAcceptableSyncKdf(const SyncKdf& kdf) {
  return kdf.n >= kMinKdf.n && kdf.n <= (1u << 20) &&
         (kdf.n & (kdf.n - 1)) == 0 && kdf.r >= kMinKdf.r && kdf.r <= 32 &&
         kdf.p >= 1 && kdf.p <= 16 && 128 * kdf.r * kdf.n <= (1u << 30);
}

SyncPasswordProblem CheckSyncPassword(std::string_view password) {
  const size_t length = CountCharacters(password);
  if (length < kMinSyncPasswordLength) {
    return SyncPasswordProblem::kTooShort;
  }
  if (length > kMaxSyncPasswordLength) {
    return SyncPasswordProblem::kTooLong;
  }
  const std::string lower = base::ToLowerASCII(password);
  if (std::ranges::find(kCommonPasswords, lower) !=
          std::end(kCommonPasswords) ||
      IsRepetitive(lower)) {
    return SyncPasswordProblem::kCommon;
  }
  if (lower.find("alerse") != std::string::npos ||
      // The product's earlier name (kept literal on purpose).
      lower.find("arcomium") != std::string::npos) {
    return SyncPasswordProblem::kContainsName;
  }
  return SyncPasswordProblem::kNone;
}

std::vector<uint8_t> NewSyncSalt() {
  std::vector<uint8_t> salt(kSaltSize);
  base::RandBytes(salt);
  return salt;
}

std::optional<SyncAccountKeys> DeriveSyncAccountKeys(
    const SyncChainSecret& sk,
    std::string_view password,
    base::span<const uint8_t> salt,
    const SyncKdf& kdf) {
  if (!IsAcceptableSyncKdf(kdf)) {
    return std::nullopt;
  }
  // pwkey | HKDF(sk, mix): the password's key first, then the words'.
  std::array<uint8_t, 64> mixed;
  // A little over the memory scrypt needs, which IsAcceptableSyncKdf bounds
  // by 1 GiB.
  const size_t max_mem = 128 * kdf.r * (kdf.n + kdf.p + 2) + (1 << 20);
  if (!EVP_PBE_scrypt(password.data(), password.size(), salt.data(),
                      salt.size(), kdf.n, kdf.r, kdf.p, max_mem, mixed.data(),
                      32)) {
    return std::nullopt;
  }
  base::span(mixed).last(32u).copy_from(Hkdf(sk, kMixInfo));
  std::array<uint8_t, 32> master = Hkdf(mixed, kMasterInfo);
  SyncAccountKeys keys{SyncAccountId(sk), Hkdf(master, kAuthInfo),
                       Hkdf(master, kWrapInfo)};
  std::ranges::fill(mixed, 0);
  std::ranges::fill(master, 0);
  return keys;
}

std::vector<uint8_t> WrapSyncAccountKey(const SyncAccountKeys& keys,
                                        const SyncChainSecret& account_key) {
  bssl::ScopedEVP_AEAD_CTX ctx;
  InitWrapCipher(ctx.get(), keys);
  std::vector<uint8_t> out(1 + kNonceSize + account_key.size() +
                           EVP_AEAD_max_overhead(EVP_aead_aes_256_gcm()));
  out[0] = kWrapVersion;
  base::span<uint8_t> nonce = base::span(out).subspan(1u, kNonceSize);
  base::RandBytes(nonce);
  const std::string ad = AdditionalData(keys.id);
  base::span<uint8_t> body = base::span(out).subspan(1u + kNonceSize);
  size_t sealed = 0;
  CHECK(EVP_AEAD_CTX_seal(ctx.get(), body.data(), &sealed, body.size(),
                          nonce.data(), nonce.size(), account_key.data(),
                          account_key.size(),
                          reinterpret_cast<const uint8_t*>(ad.data()),
                          ad.size()));
  out.resize(1 + kNonceSize + sealed);
  return out;
}

std::optional<SyncChainSecret> UnwrapSyncAccountKey(
    const SyncAccountKeys& keys,
    base::span<const uint8_t> wrapped) {
  if (wrapped.size() <= 1 + kNonceSize || wrapped[0] != kWrapVersion) {
    return std::nullopt;
  }
  bssl::ScopedEVP_AEAD_CTX ctx;
  InitWrapCipher(ctx.get(), keys);
  const base::span<const uint8_t> nonce = wrapped.subspan(1u, kNonceSize);
  const base::span<const uint8_t> sealed = wrapped.subspan(1u + kNonceSize);
  const std::string ad = AdditionalData(keys.id);
  std::vector<uint8_t> plain(sealed.size());
  size_t opened = 0;
  if (!EVP_AEAD_CTX_open(ctx.get(), plain.data(), &opened, plain.size(),
                         nonce.data(), nonce.size(), sealed.data(),
                         sealed.size(),
                         reinterpret_cast<const uint8_t*>(ad.data()),
                         ad.size()) ||
      opened != std::tuple_size_v<SyncChainSecret>) {
    return std::nullopt;
  }
  SyncChainSecret key;
  base::span(key).copy_from(base::span(plain).first(key.size()));
  return key;
}

}  // namespace syncer
