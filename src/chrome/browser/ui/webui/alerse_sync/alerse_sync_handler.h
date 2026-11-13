// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ALERSE_SYNC_ALERSE_SYNC_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ALERSE_SYNC_ALERSE_SYNC_HANDLER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/alerse_sync/alerse_sync_api_client.h"
#include "components/sync/service/alerse_sync_account.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "url/gurl.h"

class Profile;

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace os_crypt_async {
class Encryptor;
}  // namespace os_crypt_async

// Backs chrome://alerse-sync: an account of an alerse-sync server that opens
// with the 24 words and a password (docs/adr/0002-sync-accounts.md). The page
// calls:
//
//   getState()                          -> state
//   checkPassword(password)             -> {problem}
//   createAccount(server, password, confirm) -> {words}
//   signIn(server, words, password)     -> {}
//   showWords()                         -> {words}
//   getDevices()                        -> {devices: [{id, name, created,
//                                           lastSeen, current}]}
//   removeDevice(id, password)          -> {}
//   leave()                             -> {}
//   deleteAccount(password)             -> {}
//   dismissLegacy()                     -> {}
//   restart()
//
// and gets "state-changed" with the state whenever sync changes.
class AlerseSyncHandler : public content::WebUIMessageHandler,
                          public syncer::SyncServiceObserver {
 public:
  explicit AlerseSyncHandler(Profile* profile);
  AlerseSyncHandler(const AlerseSyncHandler&) = delete;
  AlerseSyncHandler& operator=(const AlerseSyncHandler&) = delete;
  ~AlerseSyncHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

 private:
  using Setup = syncer::AlerseSyncAccount::Setup;

  void HandleGetState(const base::ListValue& args);
  void HandleCheckPassword(const base::ListValue& args);
  void HandleCreateAccount(const base::ListValue& args);
  void HandleSignIn(const base::ListValue& args);
  void HandleShowWords(const base::ListValue& args);
  void HandleGetDevices(const base::ListValue& args);
  void HandleRemoveDevice(const base::ListValue& args);
  void HandleLeave(const base::ListValue& args);
  void HandleDeleteAccount(const base::ListValue& args);
  void HandleDismissLegacy(const base::ListValue& args);
  void HandleRestart(const base::ListValue& args);

  base::DictValue BuildState() const;

  // Checks that `server` answers, then runs `next`; rejects `callback_id`
  // if it does not.
  void CheckServer(const std::string& callback_id,
                   const GURL& server,
                   base::OnceClosure next);
  void OnServerChecked(const std::string& callback_id,
                       base::OnceClosure next,
                       std::optional<std::string> body);

  // Runs `next` with this device's name.
  void WithDeviceName(base::OnceCallback<void(const std::string&)> next);

  // Ends a sign-in: saves `setup` and resolves `callback_id` with `result`.
  void OnSignedIn(const std::string& callback_id,
                  const GURL& server,
                  base::DictValue result,
                  std::optional<Setup> setup,
                  std::optional<std::string> error);
  void OnEncryptorForSave(const std::string& callback_id,
                          const GURL& server,
                          Setup setup,
                          base::DictValue result,
                          scoped_refptr<os_crypt_async::Encryptor> encryptor);

  // Runs `next` with this device's setup, or rejects `callback_id`.
  void WithSetup(const std::string& callback_id,
                 base::OnceCallback<void(const Setup&)> next);
  void OnEncryptorForSetup(const std::string& callback_id,
                           base::OnceCallback<void(const Setup&)> next,
                           scoped_refptr<os_crypt_async::Encryptor> encryptor);

  // Forgets the setup on this device and stops sync.
  void ForgetSetup();

  AlerseSyncApiClient& api();
  syncer::SyncService* GetSyncService() const;

  const raw_ptr<Profile> profile_;
  std::unique_ptr<network::SimpleURLLoader> health_check_;
  std::unique_ptr<AlerseSyncApiClient> api_;
  // Set when the setup changed since the browser started.
  bool restart_needed_ = false;
  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_observation_{this};
  base::WeakPtrFactory<AlerseSyncHandler> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_ALERSE_SYNC_ALERSE_SYNC_HANDLER_H_
