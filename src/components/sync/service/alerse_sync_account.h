// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_ALERSE_SYNC_ACCOUNT_H_
#define COMPONENTS_SYNC_SERVICE_ALERSE_SYNC_ACCOUNT_H_

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/sync/service/alerse_sync_code.h"
#include "components/sync/service/alerse_sync_sk.h"
#include "url/gurl.h"

class PrefRegistrySimple;
class PrefService;

namespace os_crypt_async {
class Encryptor;
}  // namespace os_crypt_async

namespace syncer {

// Alerse syncs without a Google account, with an account of an alerse-sync
// server that opens with the 24 words and a password (scheme sk-v1,
// alerse_sync_sk.h; docs/adr/0002-sync-accounts.md). The account has a
// 32-byte key, from which every Space's sync chain derives an Ed25519 key
// pair, which names the chain, and a custom passphrase, which encrypts all
// data before it leaves the device. Each device has an Ed25519 key of its
// own, which signs its requests, so the server can refuse a removed device.
//
// On desktop a Space is a profile (docs/adr/0001-space-profiles.md), and the
// user sets sync up once for all of them: the server and the account are the
// browser's, kept in Local State, the keys encrypted with OSCrypt. Home syncs
// as the chain of the account key. Every other Space has an id and syncs as a
// chain of its own, whose key is derived from the account key and the id.
// The functions below still take a profile's prefs and find the browser's
// through PrepareProfile(); where that was never called the profile's prefs
// hold everything.
class AlerseSyncAccount {
 public:
  static constexpr size_t kSecretSize = 32;

  // What a device keeps of its account.
  struct Setup {
    // The server's account ID, 64 lowercase hex digits.
    std::string account_id;
    SyncChainSecret account_key;
    // The seed of this device's Ed25519 key.
    SyncChainSecret device_seed;
    // The secret key the 24 words encode: with the password it removes
    // devices and the account, and the recovery sheet shows it.
    SyncChainSecret words;
  };

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Call before the sync service of a profile is built, with the name of
  // the profile's directory. `local_state` then holds the setup of every
  // profile. A profile other than the first gets its Space's id here if it
  // has none: the one SetSpaceIdOfNewProfile() left for it, else a new one.
  // Forgets a setup of the scheme before accounts (0.1.13 and earlier),
  // whose chains the server no longer serves.
  static void PrepareProfile(PrefService* local_state,
                             PrefService* profile_prefs,
                             const std::string& profile_dir);

  // Makes the profile in `profile_dir` the Space `space_id`, one that
  // another device made, from its next PrepareProfile() on: a profile about
  // to be created for it, or one of this device's that takes it over.
  static void SetSpaceIdOfNewProfile(PrefService* local_state,
                                     const std::string& profile_dir,
                                     const std::string& space_id);

  // The ids of Spaces deleted on this device, kept until home's list of
  // Spaces takes them (chrome/browser/ui/tabs/alerse_space_list.h).
  static void AddDeletedSpace(PrefService* local_state,
                              const std::string& space_id);
  static std::vector<std::string> TakeDeletedSpaces(PrefService* local_state);

  // The id of the profile's Space; empty for home.
  static std::string GetSpaceId(const PrefService& profile_prefs);

  // Copies --alerse-sync-url into prefs (not in official builds). Call
  // before SyncPrefs reads whether sync is enabled.
  static void ApplyCommandLine(PrefService* prefs);

  // The server's base URL ("https://sync.alerse.app"), or an empty one when
  // the browser does not sync.
  static GURL GetServerURL(const PrefService& prefs);

  // Saves the setup: the server and the account, its keys encrypted. Sync
  // starts with it at the next browser start. Returns false and saves
  // nothing when the OS keystore cannot encrypt the keys.
  static bool Save(PrefService* prefs,
                   const os_crypt_async::Encryptor& encryptor,
                   const GURL& server_url,
                   const Setup& setup);

  // Forgets the setup. Sync stops at the next browser start.
  static void Clear(PrefService* prefs);

  // This installation's device key, kept across Clear() so that signing in
  // again does not make the account another device.
  static std::optional<SyncChainSecret> LoadDeviceSeed(
      const PrefService& prefs,
      const os_crypt_async::Encryptor& encryptor);

  // Reads the saved setup, or nullopt.
  static std::optional<Setup> LoadSetup(
      const PrefService& prefs,
      const os_crypt_async::Encryptor& encryptor);

  // Set when this device signs in to an account that may have Spaces of its
  // own: until home's first sync brings the account's list of Spaces, only
  // home syncs, so that a Space of this device with the name of one of the
  // account's takes that one's id rather than becoming a second Space of
  // that name (chrome/browser/ui/tabs/alerse_space_list.h).
  static bool IsJoining(const PrefService& prefs);
  static void SetJoining(PrefService* prefs, bool joining);

  // Whether a setup of the scheme before accounts was forgotten at startup,
  // for the Sync page to say why sync is off; ClearLegacyReset() once said.
  static bool WasLegacyReset(const PrefService& prefs);
  static void ClearLegacyReset(PrefService* prefs);

  // This device's public key, 32 bytes, from its seed.
  static std::array<uint8_t, 32> DevicePublicKey(
      const SyncChainSecret& device_seed);

  // A token of the device for the server's /v3, valid from `now` for a day:
  // base64url(0x02 | account (32) | device public key (32) | issued at (ms,
  // big-endian) | signature), signed over "alerse-sync-device-token-v1" |
  // account | public key | issued at.
  static std::string MakeDeviceToken(const Setup& setup, base::Time now);

  // The account of the profile's chain, or nullopt when there is none, it
  // cannot be decrypted, or the profile is a Space other than home while
  // the device joins.
  static std::optional<AlerseSyncAccount> Load(
      PrefService* prefs,
      const os_crypt_async::Encryptor& encryptor);

  AlerseSyncAccount(const Setup& setup,
                    const std::string& space_id,
                    const GURL& server_url);
  AlerseSyncAccount(const AlerseSyncAccount&);
  AlerseSyncAccount& operator=(const AlerseSyncAccount&);
  ~AlerseSyncAccount();

  // The device's token, as MakeDeviceToken().
  std::string MakeToken(base::Time now) const;

  // The chain's sync URL: <server>/v3/chains/<chain ID>, to which Chromium
  // adds /command/ and the events client /events.
  const GURL& sync_url() const { return sync_url_; }

  // The custom passphrase of the chain's data.
  const std::string& passphrase() const { return passphrase_; }

 private:
  Setup setup_;
  GURL sync_url_;
  std::string passphrase_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_ALERSE_SYNC_ACCOUNT_H_
