// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/alerse_sync_account.h"

#include <string_view>
#include <vector>

#include "base/base64.h"
#include "base/base64url.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/numerics/byte_conversions.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/hkdf.h"

namespace syncer {

namespace {

constexpr char kServerURLPref[] = "arcomium.sync.server_url";
constexpr char kSecretPref[] = "arcomium.sync.secret";
constexpr char kLoginPref[] = "arcomium.sync.login";
// Profile pref: the id of the profile's Space, which names its chain.
constexpr char kSpacePref[] = "alerse.sync.space";
constexpr char kHomeSpace[] = "home";
// The directory of the first profile, which is home.
constexpr char kHomeProfileDir[] = "Default";
// Local State: {profile directory: Space id} for profiles about to be made
// for Spaces of other devices.
constexpr char kNewSpacesPref[] = "alerse.sync.new_spaces";
// Local State: ids of Spaces deleted on this device.
constexpr char kDeletedSpacesPref[] = "alerse.sync.deleted_spaces";

// For setting up a device by hand until the settings page exists. The secret
// on a command line is visible to other programs of the user, so the switch
// is for testing only.
constexpr char kServerURLSwitch[] = "alerse-sync-url";
constexpr char kSecretSwitch[] = "alerse-sync-secret";

constexpr uint8_t kTokenVersion = 0x01;
constexpr std::string_view kTokenContext = "arcomium-sync-token-v1";
// The key derivation salt every sync chain was made with: never changes.
constexpr std::string_view kKdfSalt = "arcomium-sync";
constexpr std::string_view kKeyInfo = "ed25519-key";
constexpr std::string_view kPassphraseInfo = "nigori-passphrase";
// Followed by the Space's id: the secret of a Space's chain.
constexpr std::string_view kSpaceInfo = "space-chain:";

// Local State once PrepareProfile() ran.
PrefService* g_local_state = nullptr;

// The prefs that hold the setup `prefs`, a profile's, syncs with.
PrefService* SetupPrefs(PrefService* prefs) {
  return g_local_state ? g_local_state : prefs;
}
const PrefService& SetupPrefs(const PrefService& prefs) {
  return g_local_state ? *g_local_state : prefs;
}

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
void AlerseSyncAccount::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  RegisterLocalStatePrefs(registry);
  registry->RegisterStringPref(kSpacePref, std::string());
}

// static
void AlerseSyncAccount::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kServerURLPref, std::string());
  registry->RegisterStringPref(kSecretPref, std::string());
  registry->RegisterStringPref(kLoginPref, std::string());
  registry->RegisterDictionaryPref(kNewSpacesPref);
  registry->RegisterListPref(kDeletedSpacesPref);
}

// static
void AlerseSyncAccount::PrepareProfile(PrefService* local_state,
                                         PrefService* profile_prefs,
                                         const std::string& profile_dir) {
  g_local_state = local_state;
  if (profile_dir == kHomeProfileDir) {
    profile_prefs->SetString(kSpacePref, kHomeSpace);
    return;
  }
  const std::string* given =
      local_state->GetDict(kNewSpacesPref).FindString(profile_dir);
  if (profile_prefs->GetString(kSpacePref).empty()) {
    profile_prefs->SetString(
        kSpacePref,
        given ? *given : base::HexEncode(base::RandBytesAsVector(16)));
  }
  if (given) {
    ScopedDictPrefUpdate(local_state, kNewSpacesPref)->Remove(profile_dir);
  }
}

// static
void AlerseSyncAccount::SetSpaceIdOfNewProfile(PrefService* local_state,
                                                 const std::string& profile_dir,
                                                 const std::string& space_id) {
  ScopedDictPrefUpdate(local_state, kNewSpacesPref)
      ->Set(profile_dir, space_id);
}

// static
void AlerseSyncAccount::AddDeletedSpace(PrefService* local_state,
                                          const std::string& space_id) {
  ScopedListPrefUpdate(local_state, kDeletedSpacesPref)->Append(space_id);
}

// static
std::vector<std::string> AlerseSyncAccount::TakeDeletedSpaces(
    PrefService* local_state) {
  std::vector<std::string> spaces;
  for (const base::Value& space : local_state->GetList(kDeletedSpacesPref)) {
    if (space.is_string()) {
      spaces.push_back(space.GetString());
    }
  }
  if (!spaces.empty()) {
    local_state->ClearPref(kDeletedSpacesPref);
  }
  return spaces;
}

// static
std::string AlerseSyncAccount::GetSpaceId(const PrefService& profile_prefs) {
  const std::string space = profile_prefs.GetString(kSpacePref);
  return space == kHomeSpace ? std::string() : space;
}

// static
void AlerseSyncAccount::ApplyCommandLine(PrefService* prefs) {
  prefs = SetupPrefs(prefs);
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
GURL AlerseSyncAccount::GetServerURL(const PrefService& prefs) {
  GURL url(SetupPrefs(prefs).GetString(kServerURLPref));
  return url.is_valid() ? url : GURL();
}

// static
bool AlerseSyncAccount::Save(PrefService* prefs,
                               const os_crypt_async::Encryptor& encryptor,
                               const GURL& server_url,
                               const SyncChainSecret& secret) {
  prefs = SetupPrefs(prefs);
  std::string ciphertext;
  // Encryption fails only when the OS keystore is unavailable; then nothing
  // is kept, so the profile is not left with a server and no secret.
  if (!encryptor.EncryptString(std::string(secret.begin(), secret.end()),
                               &ciphertext)) {
    LOG(ERROR) << "Cannot encrypt the Alerse sync secret";
    return false;
  }
  prefs->SetString(kSecretPref, base::Base64Encode(ciphertext));
  prefs->SetString(kServerURLPref, server_url.spec());
  return true;
}

// static
void AlerseSyncAccount::Clear(PrefService* prefs) {
  prefs = SetupPrefs(prefs);
  prefs->ClearPref(kServerURLPref);
  prefs->ClearPref(kSecretPref);
  ClearLogin(prefs);
}

// static
std::string AlerseSyncAccount::GetLogin(const PrefService& prefs) {
  return SetupPrefs(prefs).GetString(kLoginPref);
}

// static
void AlerseSyncAccount::SetLogin(PrefService* prefs,
                                   const std::string& login) {
  SetupPrefs(prefs)->SetString(kLoginPref, login);
}

// static
void AlerseSyncAccount::ClearLogin(PrefService* prefs) {
  SetupPrefs(prefs)->ClearPref(kLoginPref);
}

// static
std::optional<SyncChainSecret> AlerseSyncAccount::LoadSecret(
    const PrefService& prefs,
    const os_crypt_async::Encryptor& encryptor) {
  std::string ciphertext;
  std::string plaintext;
  if (!base::Base64Decode(SetupPrefs(prefs).GetString(kSecretPref),
                          &ciphertext) ||
      ciphertext.empty() || !encryptor.DecryptString(ciphertext, &plaintext) ||
      plaintext.size() != kSecretSize) {
    return std::nullopt;
  }
  SyncChainSecret secret;
  base::span(secret).copy_from(base::as_byte_span(plaintext));
  return secret;
}

// static
std::optional<AlerseSyncAccount> AlerseSyncAccount::Load(
    PrefService* prefs,
    const os_crypt_async::Encryptor& encryptor) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
#if defined(OFFICIAL_BUILD)
  const bool secret_switch = false;
#else
  const bool secret_switch = command_line.HasSwitch(kSecretSwitch);
#endif
  std::optional<SyncChainSecret> secret;
  if (secret_switch) {
    secret.emplace();
    if (!base::HexStringToSpan(command_line.GetSwitchValueASCII(kSecretSwitch),
                               *secret)) {
      LOG(ERROR) << "--" << kSecretSwitch << " needs 64 hex digits";
      return std::nullopt;
    }
    Save(prefs, encryptor, GetServerURL(*prefs), *secret);
  } else {
    secret = LoadSecret(*prefs, encryptor);
  }
  if (!secret) {
    return std::nullopt;
  }
  const std::string space = GetSpaceId(*prefs);
  if (space.empty()) {
    return AlerseSyncAccount(*secret);
  }
  return AlerseSyncAccount(Derive(*secret, std::string(kSpaceInfo) + space));
}

AlerseSyncAccount::AlerseSyncAccount(const SyncChainSecret& secret) {
  const std::array<uint8_t, 32> seed = Derive(secret, kKeyInfo);
  ED25519_keypair_from_seed(public_key_.data(), private_key_.data(),
                            seed.data());
  passphrase_ = base::Base64Encode(Derive(secret, kPassphraseInfo));
}

AlerseSyncAccount::AlerseSyncAccount(const AlerseSyncAccount&) = default;
AlerseSyncAccount& AlerseSyncAccount::operator=(
    const AlerseSyncAccount&) = default;
AlerseSyncAccount::~AlerseSyncAccount() = default;

std::string AlerseSyncAccount::MakeToken(base::Time now) const {
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
