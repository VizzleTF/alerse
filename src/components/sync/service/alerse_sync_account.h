// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_ALERSE_SYNC_ACCOUNT_H_
#define COMPONENTS_SYNC_SERVICE_ALERSE_SYNC_ACCOUNT_H_

#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include "base/time/time.h"
#include "components/sync/service/alerse_sync_code.h"
#include "url/gurl.h"

class PrefRegistrySimple;
class PrefService;

namespace os_crypt_async {
class Encryptor;
}  // namespace os_crypt_async

namespace syncer {

// Alerse syncs without a Google account. The devices of a sync chain share
// a 32-byte secret, from which each derives the same Ed25519 key pair, which
// is the chain's identity on the server, and the same custom passphrase,
// which encrypts all data before it leaves the device.
//
// The server URL lives in prefs; the secret is kept encrypted with OSCrypt.
// Sync runs in Chromium's local sync mode (no account, full sync) against the
// server, with tokens signed by the chain's key as access tokens.
class AlerseSyncAccount {
 public:
  static constexpr size_t kSecretSize = 32;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Copies --alerse-sync-url into prefs. Call before SyncPrefs reads
  // whether sync is enabled.
  static void ApplyCommandLine(PrefService* prefs);

  // The server's URL, or an empty one when the profile does not sync.
  static GURL GetServerURL(const PrefService& prefs);

  // Saves a profile's setup: the server and the chain's secret, encrypted.
  // Sync starts with it at the next browser start.
  static void Save(PrefService* prefs,
                   const os_crypt_async::Encryptor& encryptor,
                   const GURL& server_url,
                   const SyncChainSecret& secret);

  // Forgets the setup, including the sign-in's local settings. Sync stops at
  // the next browser start.
  static void Clear(PrefService* prefs);

  // The optional sign-in with a login name and password, as this device
  // knows it: the normalized login name it set or joined with, or empty.
  // The password is never kept.
  static std::string GetLogin(const PrefService& prefs);
  static void SetLogin(PrefService* prefs, const std::string& login);
  static void ClearLogin(PrefService* prefs);

  // Reads the saved secret, or nullopt.
  static std::optional<SyncChainSecret> LoadSecret(
      const PrefService& prefs,
      const os_crypt_async::Encryptor& encryptor);

  // Reads the chain's secret: from --alerse-sync-secret (64 hex digits),
  // which is then saved, or from prefs. Returns nullopt when there is none or
  // it cannot be decrypted.
  static std::optional<AlerseSyncAccount> Load(
      PrefService* prefs,
      const os_crypt_async::Encryptor& encryptor);

  explicit AlerseSyncAccount(const SyncChainSecret& secret);
  AlerseSyncAccount(const AlerseSyncAccount&);
  AlerseSyncAccount& operator=(const AlerseSyncAccount&);
  ~AlerseSyncAccount();

  // A token for the server, valid from `now` for a day:
  // base64url(0x01 | public key | issued at (ms, big-endian) | signature),
  // signed over "arcomium-sync-token-v1" | public key | issued at.
  std::string MakeToken(base::Time now) const;

  // The custom passphrase of the chain's data.
  const std::string& passphrase() const { return passphrase_; }

 private:
  std::array<uint8_t, 32> public_key_;
  std::array<uint8_t, 64> private_key_;
  std::string passphrase_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_ALERSE_SYNC_ACCOUNT_H_
