// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/alerse_sync_account.h"

#include <algorithm>
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
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace syncer {

namespace {

constexpr char kServerPref[] = "alerse.sync.server";
constexpr char kAccountPref[] = "alerse.sync.account";
// Encrypted with OSCrypt, base64.
constexpr char kAccountKeyPref[] = "alerse.sync.account_key";
constexpr char kDeviceKeyPref[] = "alerse.sync.device_key";
constexpr char kWordsPref[] = "alerse.sync.words";
// Set when a setup of the scheme before accounts was forgotten.
constexpr char kLegacyResetPref[] = "alerse.sync.legacy_reset";
// Set while a device that signed in waits for the account's list of Spaces.
constexpr char kJoiningPref[] = "alerse.sync.joining";
// The scheme before accounts (0.1.13 and earlier): a chain secret alone.
constexpr char kLegacyServerPref[] = "arcomium.sync.server_url";
constexpr char kLegacySecretPref[] = "arcomium.sync.secret";
constexpr char kLegacyLoginPref[] = "arcomium.sync.login";
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

// The server for tests; release builds take it only from the Sync page.
constexpr char kServerURLSwitch[] = "alerse-sync-url";

constexpr uint8_t kTokenVersion = 0x02;
constexpr std::string_view kTokenContext = "alerse-sync-device-token-v1";
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
  registry->RegisterStringPref(kServerPref, std::string());
  registry->RegisterStringPref(kAccountPref, std::string());
  registry->RegisterStringPref(kAccountKeyPref, std::string());
  registry->RegisterStringPref(kDeviceKeyPref, std::string());
  registry->RegisterStringPref(kWordsPref, std::string());
  registry->RegisterBooleanPref(kLegacyResetPref, false);
  registry->RegisterBooleanPref(kJoiningPref, false);
  registry->RegisterStringPref(kLegacyServerPref, std::string());
  registry->RegisterStringPref(kLegacySecretPref, std::string());
  registry->RegisterStringPref(kLegacyLoginPref, std::string());
  registry->RegisterDictionaryPref(kNewSpacesPref);
  registry->RegisterListPref(kDeletedSpacesPref);
}

// static
void AlerseSyncAccount::PrepareProfile(PrefService* local_state,
                                         PrefService* profile_prefs,
                                         const std::string& profile_dir) {
  g_local_state = local_state;
  // The server no longer serves chains without an account: sync is set up
  // again, and the page says why.
  for (PrefService* prefs : {local_state, profile_prefs}) {
    if (!prefs->GetString(kLegacyServerPref).empty() ||
        !prefs->GetString(kLegacySecretPref).empty()) {
      prefs->ClearPref(kLegacyServerPref);
      prefs->ClearPref(kLegacySecretPref);
      prefs->ClearPref(kLegacyLoginPref);
      local_state->SetBoolean(kLegacyResetPref, true);
    }
  }
  if (profile_dir == kHomeProfileDir) {
    profile_prefs->SetString(kSpacePref, kHomeSpace);
    return;
  }
  const std::string* given =
      local_state->GetDict(kNewSpacesPref).FindString(profile_dir);
  // A given id wins: the profile is made for, or takes over, a Space of
  // another device.
  if (given) {
    profile_prefs->SetString(kSpacePref, *given);
  } else if (profile_prefs->GetString(kSpacePref).empty()) {
    profile_prefs->SetString(kSpacePref,
                             base::HexEncode(base::RandBytesAsVector(16)));
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
  // Release builds take the server only from the Sync page.
  return;
#endif
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(kServerURLSwitch)) {
    prefs->SetString(kServerPref,
                     command_line.GetSwitchValueASCII(kServerURLSwitch));
  }
}

// static
GURL AlerseSyncAccount::GetServerURL(const PrefService& prefs) {
  GURL url(SetupPrefs(prefs).GetString(kServerPref));
  return url.is_valid() ? url : GURL();
}

namespace {

bool Encrypt(const os_crypt_async::Encryptor& encryptor,
             const SyncChainSecret& secret,
             std::string* out) {
  std::string ciphertext;
  if (!encryptor.EncryptString(std::string(secret.begin(), secret.end()),
                               &ciphertext)) {
    return false;
  }
  *out = base::Base64Encode(ciphertext);
  return true;
}

std::optional<SyncChainSecret> Decrypt(
    const os_crypt_async::Encryptor& encryptor,
    const std::string& stored) {
  std::string ciphertext;
  std::string plaintext;
  if (!base::Base64Decode(stored, &ciphertext) || ciphertext.empty() ||
      !encryptor.DecryptString(ciphertext, &plaintext) ||
      plaintext.size() != AlerseSyncAccount::kSecretSize) {
    return std::nullopt;
  }
  SyncChainSecret secret;
  base::span(secret).copy_from(base::as_byte_span(plaintext));
  return secret;
}

bool IsAccountId(const std::string& id) {
  std::array<uint8_t, 32> raw;
  return id.size() == 64 && base::HexStringToSpan(id, raw) &&
         base::HexEncodeLower(raw) == id;
}

}  // namespace

// static
bool AlerseSyncAccount::Save(PrefService* prefs,
                               const os_crypt_async::Encryptor& encryptor,
                               const GURL& server_url,
                               const Setup& setup) {
  prefs = SetupPrefs(prefs);
  std::string account_key;
  std::string device_key;
  std::string words;
  // Encryption fails only when the OS keystore is unavailable; then nothing
  // is kept, so the browser is not left with a server and no keys.
  if (!IsAccountId(setup.account_id) ||
      !Encrypt(encryptor, setup.account_key, &account_key) ||
      !Encrypt(encryptor, setup.device_seed, &device_key) ||
      !Encrypt(encryptor, setup.words, &words)) {
    LOG(ERROR) << "Cannot keep the Alerse sync account";
    return false;
  }
  prefs->SetString(kAccountPref, setup.account_id);
  prefs->SetString(kAccountKeyPref, account_key);
  prefs->SetString(kDeviceKeyPref, device_key);
  prefs->SetString(kWordsPref, words);
  prefs->SetString(kServerPref, server_url.spec());
  prefs->ClearPref(kLegacyResetPref);
  return true;
}

// static
void AlerseSyncAccount::Clear(PrefService* prefs) {
  prefs = SetupPrefs(prefs);
  for (const char* pref : {kServerPref, kAccountPref, kAccountKeyPref,
                           kDeviceKeyPref, kWordsPref, kJoiningPref}) {
    prefs->ClearPref(pref);
  }
}

// static
std::optional<AlerseSyncAccount::Setup> AlerseSyncAccount::LoadSetup(
    const PrefService& prefs,
    const os_crypt_async::Encryptor& encryptor) {
  const PrefService& setup_prefs = SetupPrefs(prefs);
  Setup setup;
  setup.account_id = setup_prefs.GetString(kAccountPref);
  std::optional<SyncChainSecret> account_key =
      Decrypt(encryptor, setup_prefs.GetString(kAccountKeyPref));
  std::optional<SyncChainSecret> device_seed =
      Decrypt(encryptor, setup_prefs.GetString(kDeviceKeyPref));
  std::optional<SyncChainSecret> words =
      Decrypt(encryptor, setup_prefs.GetString(kWordsPref));
  if (!IsAccountId(setup.account_id) || !account_key || !device_seed ||
      !words) {
    return std::nullopt;
  }
  setup.account_key = *account_key;
  setup.device_seed = *device_seed;
  setup.words = *words;
  return setup;
}

// static
bool AlerseSyncAccount::IsJoining(const PrefService& prefs) {
  return SetupPrefs(prefs).GetBoolean(kJoiningPref);
}

// static
void AlerseSyncAccount::SetJoining(PrefService* prefs, bool joining) {
  SetupPrefs(prefs)->SetBoolean(kJoiningPref, joining);
}

// static
bool AlerseSyncAccount::WasLegacyReset(const PrefService& prefs) {
  return SetupPrefs(prefs).GetBoolean(kLegacyResetPref);
}

// static
void AlerseSyncAccount::ClearLegacyReset(PrefService* prefs) {
  SetupPrefs(prefs)->ClearPref(kLegacyResetPref);
}

// static
std::array<uint8_t, 32> AlerseSyncAccount::DevicePublicKey(
    const SyncChainSecret& device_seed) {
  std::array<uint8_t, 32> public_key;
  std::array<uint8_t, 64> private_key;
  ED25519_keypair_from_seed(public_key.data(), private_key.data(),
                            device_seed.data());
  std::ranges::fill(private_key, 0);
  return public_key;
}

// static
std::string AlerseSyncAccount::MakeDeviceToken(const Setup& setup,
                                                 base::Time now) {
  std::array<uint8_t, 32> public_key;
  std::array<uint8_t, 64> private_key;
  ED25519_keypair_from_seed(public_key.data(), private_key.data(),
                            setup.device_seed.data());
  std::array<uint8_t, 32> account;
  CHECK(base::HexStringToSpan(setup.account_id, account));

  std::vector<uint8_t> token;
  token.push_back(kTokenVersion);
  token.insert(token.end(), account.begin(), account.end());
  token.insert(token.end(), public_key.begin(), public_key.end());
  const auto issued_at = base::U64ToBigEndian(
      static_cast<uint64_t>(now.InMillisecondsSinceUnixEpoch()));
  token.insert(token.end(), issued_at.begin(), issued_at.end());

  // Signed: the context, then the account, key and time (the token without
  // its version byte).
  std::vector<uint8_t> message(kTokenContext.begin(), kTokenContext.end());
  message.insert(message.end(), token.begin() + 1, token.end());
  std::array<uint8_t, 64> signature;
  CHECK(ED25519_sign(signature.data(), message.data(), message.size(),
                     private_key.data()));
  std::ranges::fill(private_key, 0);
  token.insert(token.end(), signature.begin(), signature.end());

  std::string encoded;
  base::Base64UrlEncode(token, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded);
  return encoded;
}

// static
std::optional<AlerseSyncAccount> AlerseSyncAccount::Load(
    PrefService* prefs,
    const os_crypt_async::Encryptor& encryptor) {
  const GURL server = GetServerURL(*prefs);
  std::optional<Setup> setup = LoadSetup(*prefs, encryptor);
  const std::string space = GetSpaceId(*prefs);
  if (!server.is_valid() || !setup || (!space.empty() && IsJoining(*prefs))) {
    return std::nullopt;
  }
  return AlerseSyncAccount(*setup, space, server);
}

AlerseSyncAccount::AlerseSyncAccount(const Setup& setup,
                                         const std::string& space_id,
                                         const GURL& server_url)
    : setup_(setup) {
  // Home's chain derives from the account key, every other Space's from a
  // key derived from it and the Space's id.
  const SyncChainSecret chain_secret =
      space_id.empty()
          ? setup.account_key
          : Derive(setup.account_key, std::string(kSpaceInfo) + space_id);
  const std::array<uint8_t, 32> seed = Derive(chain_secret, kKeyInfo);
  std::array<uint8_t, 32> chain_public;
  std::array<uint8_t, 64> chain_private;
  ED25519_keypair_from_seed(chain_public.data(), chain_private.data(),
                            seed.data());
  std::ranges::fill(chain_private, 0);
  std::array<uint8_t, SHA256_DIGEST_LENGTH> chain_id;
  SHA256(chain_public.data(), chain_public.size(), chain_id.data());
  std::string base = server_url.spec();
  if (!base.ends_with('/')) {
    base += '/';
  }
  sync_url_ = GURL(base + "v3/chains/" + base::HexEncodeLower(chain_id));
  passphrase_ = base::Base64Encode(Derive(chain_secret, kPassphraseInfo));
}

AlerseSyncAccount::AlerseSyncAccount(const AlerseSyncAccount&) = default;
AlerseSyncAccount& AlerseSyncAccount::operator=(
    const AlerseSyncAccount&) = default;
AlerseSyncAccount::~AlerseSyncAccount() = default;

std::string AlerseSyncAccount::MakeToken(base::Time now) const {
  return MakeDeviceToken(setup_, now);
}

}  // namespace syncer
