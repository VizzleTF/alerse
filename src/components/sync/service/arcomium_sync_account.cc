// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/arcomium_sync_account.h"

#include <string_view>
#include <vector>

#include "base/base64.h"
#include "base/base64url.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/numerics/byte_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/hkdf.h"

namespace syncer {

namespace {

constexpr char kServerURLPref[] = "arcomium.sync.server_url";
constexpr char kSecretPref[] = "arcomium.sync.secret";

// For setting up a device by hand until the settings page exists. The secret
// on a command line is visible to other programs of the user, so the switch
// is for testing only.
constexpr char kServerURLSwitch[] = "arcomium-sync-url";
constexpr char kSecretSwitch[] = "arcomium-sync-secret";

constexpr uint8_t kTokenVersion = 0x01;
constexpr std::string_view kTokenContext = "arcomium-sync-token-v1";
constexpr std::string_view kKdfSalt = "arcomium-sync";
constexpr std::string_view kKeyInfo = "ed25519-key";
constexpr std::string_view kPassphraseInfo = "nigori-passphrase";

std::array<uint8_t, 32> Derive(base::span<const uint8_t> secret,
                               std::string_view info) {
  std::array<uint8_t, 32> out;
  CHECK(HKDF(out.data(), out.size(), EVP_sha256(), secret.data(),
             secret.size(), reinterpret_cast<const uint8_t*>(kKdfSalt.data()),
             kKdfSalt.size(), reinterpret_cast<const uint8_t*>(info.data()),
             info.size()));
  return out;
}

}  // namespace

// static
void ArcomiumSyncAccount::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kServerURLPref, std::string());
  registry->RegisterStringPref(kSecretPref, std::string());
}

// static
void ArcomiumSyncAccount::ApplyCommandLine(PrefService* prefs) {
#if defined(OFFICIAL_BUILD)
  // A shortcut with these switches would join a profile to someone else's
  // sync chain; release builds take a server and a code only from the Sync
  // page.
  return;
#endif
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(kServerURLSwitch)) {
    prefs->SetString(kServerURLPref,
                     command_line.GetSwitchValueASCII(kServerURLSwitch));
  }
}

// static
GURL ArcomiumSyncAccount::GetServerURL(const PrefService& prefs) {
  GURL url(prefs.GetString(kServerURLPref));
  return url.is_valid() ? url : GURL();
}

// static
void ArcomiumSyncAccount::Save(PrefService* prefs,
                               const os_crypt_async::Encryptor& encryptor,
                               const GURL& server_url,
                               const SyncChainSecret& secret) {
  std::string ciphertext;
  // Encryption fails only when the OS keystore is unavailable; then the
  // secret is not kept and sync does not start.
  if (!encryptor.EncryptString(std::string(secret.begin(), secret.end()),
                               &ciphertext)) {
    LOG(ERROR) << "Cannot encrypt the Arcomium sync secret";
    ciphertext.clear();
  }
  prefs->SetString(kSecretPref, base::Base64Encode(ciphertext));
  prefs->SetString(kServerURLPref, server_url.spec());
}

// static
void ArcomiumSyncAccount::Clear(PrefService* prefs) {
  prefs->ClearPref(kServerURLPref);
  prefs->ClearPref(kSecretPref);
}

// static
std::optional<SyncChainSecret> ArcomiumSyncAccount::LoadSecret(
    const PrefService& prefs,
    const os_crypt_async::Encryptor& encryptor) {
  std::string ciphertext;
  std::string plaintext;
  if (!base::Base64Decode(prefs.GetString(kSecretPref), &ciphertext) ||
      ciphertext.empty() || !encryptor.DecryptString(ciphertext, &plaintext) ||
      plaintext.size() != kSecretSize) {
    return std::nullopt;
  }
  SyncChainSecret secret;
  base::span(secret).copy_from(base::as_byte_span(plaintext));
  return secret;
}

// static
std::optional<ArcomiumSyncAccount> ArcomiumSyncAccount::Load(
    PrefService* prefs,
    const os_crypt_async::Encryptor& encryptor) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
#if defined(OFFICIAL_BUILD)
  const bool secret_switch = false;
#else
  const bool secret_switch = command_line.HasSwitch(kSecretSwitch);
#endif
  if (secret_switch) {
    SyncChainSecret secret;
    if (!base::HexStringToSpan(command_line.GetSwitchValueASCII(kSecretSwitch),
                               secret)) {
      LOG(ERROR) << "--" << kSecretSwitch << " needs 64 hex digits";
      return std::nullopt;
    }
    Save(prefs, encryptor, GetServerURL(*prefs), secret);
    return ArcomiumSyncAccount(secret);
  }

  std::optional<SyncChainSecret> secret = LoadSecret(*prefs, encryptor);
  if (!secret) {
    return std::nullopt;
  }
  return ArcomiumSyncAccount(*secret);
}

ArcomiumSyncAccount::ArcomiumSyncAccount(const SyncChainSecret& secret) {
  const std::array<uint8_t, 32> seed = Derive(secret, kKeyInfo);
  ED25519_keypair_from_seed(public_key_.data(), private_key_.data(),
                            seed.data());
  passphrase_ = base::Base64Encode(Derive(secret, kPassphraseInfo));
}

ArcomiumSyncAccount::ArcomiumSyncAccount(const ArcomiumSyncAccount&) = default;
ArcomiumSyncAccount& ArcomiumSyncAccount::operator=(
    const ArcomiumSyncAccount&) = default;
ArcomiumSyncAccount::~ArcomiumSyncAccount() = default;

std::string ArcomiumSyncAccount::MakeToken(base::Time now) const {
  std::vector<uint8_t> token;
  token.push_back(kTokenVersion);
  token.insert(token.end(), public_key_.begin(), public_key_.end());
  const auto issued_at = base::U64ToBigEndian(
      static_cast<uint64_t>(now.InMillisecondsSinceUnixEpoch()));
  token.insert(token.end(), issued_at.begin(), issued_at.end());

  // Signed: the context, then the key and time (the token without its
  // version byte).
  std::vector<uint8_t> message(kTokenContext.begin(), kTokenContext.end());
  message.insert(message.end(), token.begin() + 1, token.end());
  std::array<uint8_t, 64> signature;
  CHECK(ED25519_sign(signature.data(), message.data(), message.size(),
                     private_key_.data()));
  token.insert(token.end(), signature.begin(), signature.end());

  std::string encoded;
  base::Base64UrlEncode(token, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded);
  return encoded;
}

}  // namespace syncer
