// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/alerse_sync_pairing.h"

#include <algorithm>

#include "base/check.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "third_party/boringssl/src/include/openssl/aead.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/hkdf.h"
#include "third_party/boringssl/src/include/openssl/hmac.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace syncer {

namespace {

constexpr std::string_view kAlphabet = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
constexpr size_t kSlotChars = 4;
constexpr size_t kPasswordChars = 8;
constexpr std::string_view kSlotContext = "arcomium-pair-slot-v1";
constexpr std::string_view kContext = "arcomium-pair-v1";
constexpr std::string_view kExistingName = "arcomium-pair-existing";
constexpr std::string_view kNewName = "arcomium-pair-new";
constexpr std::string_view kConfirmContext = "arcomium-pair-new-device";
constexpr uint8_t kBoxVersion = 0x01;
constexpr size_t kNonceSize = 12;

const uint8_t* Bytes(std::string_view s) {
  return reinterpret_cast<const uint8_t*>(s.data());
}

std::array<uint8_t, 32> Expand(base::span<const uint8_t> key,
                               std::string_view info) {
  std::array<uint8_t, 32> out;
  CHECK(HKDF(out.data(), out.size(), EVP_sha256(), key.data(), key.size(),
             nullptr, 0, Bytes(info), info.size()));
  return out;
}

}  // namespace

std::string SyncPairingCode::Display() const {
  return base::StrCat(
      {slot_part, "-", password.substr(0, 4), "-", password.substr(4)});
}

std::string SyncPairingCode::SlotId() const {
  const std::string input = base::StrCat({kSlotContext, std::string(1, '\0'),
                                          slot_part});
  std::array<uint8_t, SHA256_DIGEST_LENGTH> digest;
  SHA256(Bytes(input), input.size(), digest.data());
  return base::HexEncodeLower(digest);
}

SyncPairingCode GenerateSyncPairingCode() {
  std::array<uint8_t, kSlotChars + kPasswordChars> random;
  base::RandBytes(random);
  std::string chars;
  for (uint8_t byte : random) {
    // 256 is a multiple of 32, so every character is equally likely.
    chars.push_back(kAlphabet[byte % kAlphabet.size()]);
  }
  return {chars.substr(0, kSlotChars), chars.substr(kSlotChars)};
}

std::optional<SyncPairingCode> ParseSyncPairingCode(std::string_view typed) {
  std::string chars;
  for (char c : typed) {
    if (c == '-' || c == ' ' || c == '\t' || c == '\n' || c == '\r') {
      continue;
    }
    c = base::ToUpperASCII(c);
    if (c == 'O') {
      c = '0';
    } else if (c == 'I' || c == 'L') {
      c = '1';
    }
    if (kAlphabet.find(c) == std::string_view::npos) {
      return std::nullopt;
    }
    chars.push_back(c);
  }
  if (chars.size() != kSlotChars + kPasswordChars) {
    return std::nullopt;
  }
  return SyncPairingCode{chars.substr(0, kSlotChars),
                         chars.substr(kSlotChars)};
}

SyncPairing::SyncPairing(Role role, const SyncPairingCode& code)
    : slot_(code.SlotId()) {
  const bool existing = role == Role::kExisting;
  const std::string_view me = existing ? kExistingName : kNewName;
  const std::string_view them = existing ? kNewName : kExistingName;
  ctx_.reset(SPAKE2_CTX_new(existing ? spake2_role_alice : spake2_role_bob,
                            Bytes(me), me.size(), Bytes(them), them.size()));
  CHECK(ctx_);
  const std::string password = base::StrCat(
      {kContext, std::string(1, '\0'), slot_, std::string(1, '\0'),
       code.password});
  message_.resize(SPAKE2_MAX_MSG_SIZE);
  size_t length = 0;
  CHECK(SPAKE2_generate_msg(ctx_.get(), message_.data(), &length,
                            message_.size(), Bytes(password),
                            password.size()));
  message_.resize(length);
}

SyncPairing::~SyncPairing() = default;

bool SyncPairing::Finish(base::span<const uint8_t> their_message) {
  if (finished_ || their_message.size() != SPAKE2_MAX_MSG_SIZE) {
    return false;
  }
  std::array<uint8_t, SPAKE2_MAX_KEY_SIZE> key;
  size_t length = 0;
  if (!SPAKE2_process_msg(ctx_.get(), key.data(), &length, key.size(),
                          their_message.data(), their_message.size())) {
    return false;
  }
  const base::span<const uint8_t> material = base::span(key).first(length);
  confirm_key_ = Expand(material, "arcomium-pair-confirm");
  box_key_ = Expand(material, "arcomium-pair-box");
  verify_key_ = Expand(material, "arcomium-pair-verify");
  std::ranges::fill(key, 0);
  finished_ = true;
  return true;
}

std::string SyncPairing::VerificationNumber() const {
  CHECK(finished_);
  const uint32_t value = (uint32_t{verify_key_[0]} << 24) |
                         (uint32_t{verify_key_[1]} << 16) |
                         (uint32_t{verify_key_[2]} << 8) | verify_key_[3];
  return base::StringPrintf("%04u", value % 10000);
}

std::array<uint8_t, 32> SyncPairing::Confirm(
    std::string_view device_name) const {
  CHECK(finished_);
  const std::string input =
      base::StrCat({kConfirmContext, std::string(1, '\0'), device_name});
  std::array<uint8_t, 32> mac;
  unsigned int length = 0;
  CHECK(HMAC(EVP_sha256(), confirm_key_.data(), confirm_key_.size(),
             Bytes(input), input.size(), mac.data(), &length));
  return mac;
}

bool SyncPairing::CheckConfirm(std::string_view device_name,
                               base::span<const uint8_t> confirm) const {
  const std::array<uint8_t, 32> expected = Confirm(device_name);
  return confirm.size() == expected.size() &&
         CRYPTO_memcmp(confirm.data(), expected.data(), expected.size()) == 0;
}

std::vector<uint8_t> SyncPairing::Seal(const SyncChainSecret& secret) const {
  CHECK(finished_);
  bssl::ScopedEVP_AEAD_CTX ctx;
  CHECK(EVP_AEAD_CTX_init(ctx.get(), EVP_aead_aes_256_gcm(), box_key_.data(),
                          box_key_.size(), EVP_AEAD_DEFAULT_TAG_LENGTH,
                          nullptr));
  std::vector<uint8_t> out(1 + kNonceSize + secret.size() +
                           EVP_AEAD_max_overhead(EVP_aead_aes_256_gcm()));
  out[0] = kBoxVersion;
  base::span<uint8_t> nonce = base::span(out).subspan(1u, kNonceSize);
  base::RandBytes(nonce);
  base::span<uint8_t> body = base::span(out).subspan(1u + kNonceSize);
  const std::string ad = base::StrCat({kContext, slot_});
  size_t sealed = 0;
  CHECK(EVP_AEAD_CTX_seal(ctx.get(), body.data(), &sealed, body.size(),
                          nonce.data(), nonce.size(), secret.data(),
                          secret.size(), Bytes(ad), ad.size()));
  out.resize(1 + kNonceSize + sealed);
  return out;
}

std::optional<SyncChainSecret> SyncPairing::Open(
    base::span<const uint8_t> box) const {
  if (!finished_ || box.size() <= 1 + kNonceSize || box[0] != kBoxVersion) {
    return std::nullopt;
  }
  bssl::ScopedEVP_AEAD_CTX ctx;
  CHECK(EVP_AEAD_CTX_init(ctx.get(), EVP_aead_aes_256_gcm(), box_key_.data(),
                          box_key_.size(), EVP_AEAD_DEFAULT_TAG_LENGTH,
                          nullptr));
  const base::span<const uint8_t> nonce = box.subspan(1u, kNonceSize);
  const base::span<const uint8_t> sealed = box.subspan(1u + kNonceSize);
  const std::string ad = base::StrCat({kContext, slot_});
  std::vector<uint8_t> plain(sealed.size());
  size_t opened = 0;
  if (!EVP_AEAD_CTX_open(ctx.get(), plain.data(), &opened, plain.size(),
                         nonce.data(), nonce.size(), sealed.data(),
                         sealed.size(), Bytes(ad), ad.size()) ||
      opened != std::tuple_size_v<SyncChainSecret>) {
    return std::nullopt;
  }
  SyncChainSecret secret;
  base::span(secret).copy_from(base::span(plain).first(secret.size()));
  return secret;
}

}  // namespace syncer
