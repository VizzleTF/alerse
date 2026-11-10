// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ALERSE_SYNC_ALERSE_SYNC_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ALERSE_SYNC_ALERSE_SYNC_HANDLER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/alerse_sync/alerse_sync_login_client.h"
#include "components/sync/service/alerse_sync_code.h"
#include "components/sync/service/alerse_sync_pairing.h"
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

// Backs chrome://alerse-sync: starting or joining a sync chain, showing its
// sync code, leaving it, and the chain's optional sign-in with a login name
// and password. The page calls:
//
//   getState()                           -> state
//   createChain(server, login|null)      -> {code}
//   joinChain(server, code)              -> {}
//   joinWithLogin(server, login, password) -> {}
//   showCode()                           -> {code}
//   leaveChain()                         -> {}
//   getLoginState()                      -> {login, onServer}
//   setLogin(login)                      -> {}
//   removeLogin()                        -> {}
//   checkPassword(login, password)       -> {problem}
//   startPairing()                       -> {code, link, qr: {size, modules}}
//   pairingStatus()                      -> {state, name, number, expires}
//   approvePairing()                     -> {}
//   cancelPairing()                      -> {}
//   joinWithPairing(server, code)        -> {number}
//   pairingJoinStatus()                  -> {state}
//   restart()
//
// where login is {login, password, confirm}. Pairing: the device that syncs
// shows a one-time code (startPairing) and polls pairingStatus until the new
// device joins, then the user allows it (approvePairing); the new device
// joins with the code (joinWithPairing) and polls pairingJoinStatus until it
// has the chain's secret, which it saves like joinChain.
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
  void HandleGetState(const base::ListValue& args);
  void HandleCreateChain(const base::ListValue& args);
  void HandleJoinChain(const base::ListValue& args);
  void HandleShowCode(const base::ListValue& args);
  void HandleLeaveChain(const base::ListValue& args);
  void HandleRestart(const base::ListValue& args);
  void HandleJoinWithLogin(const base::ListValue& args);
  void HandleGetLoginState(const base::ListValue& args);
  void HandleSetLogin(const base::ListValue& args);
  void HandleRemoveLogin(const base::ListValue& args);
  void HandleCheckPassword(const base::ListValue& args);
  void HandleStartPairing(const base::ListValue& args);
  void HandlePairingStatus(const base::ListValue& args);
  void HandleApprovePairing(const base::ListValue& args);
  void HandleCancelPairing(const base::ListValue& args);
  void HandleJoinWithPairing(const base::ListValue& args);
  void HandlePairingJoinStatus(const base::ListValue& args);

  void OnPairingOpened(const std::string& callback_id,
                       AlerseSyncLoginClient::Response response);
  void OnPairingSlot(const std::string& callback_id,
                     AlerseSyncLoginClient::Response response);
  void OnPairingSlotForJoin(const std::string& callback_id,
                            AlerseSyncLoginClient::Response response);
  void OnDeviceNameForJoin(const std::string& callback_id,
                           const std::string& name);
  void OnPairingJoinSlot(const std::string& callback_id,
                         AlerseSyncLoginClient::Response response);
  void ResetPairing();

  base::DictValue BuildState() const;

  // A sign-in the user typed, checked.
  struct NewLogin {
    std::string login;  // normalized
    std::string password;
  };
  // Reads {login, password, confirm}; on a problem, rejects
  // `callback_id` and returns nullopt.
  std::optional<NewLogin> ReadNewLogin(const std::string& callback_id,
                                       const base::Value& value);

  // Checks that `server` answers, then runs `next`; rejects `callback_id`
  // if it does not.
  void CheckServer(const std::string& callback_id,
                   const GURL& server,
                   base::OnceClosure next);
  void OnServerChecked(const std::string& callback_id,
                       base::OnceClosure next,
                       std::optional<std::string> body);

  // Saves the setup with `secret` and, if set, the sign-in's local settings,
  // then resolves `callback_id` with `result`.
  void Save(const std::string& callback_id,
            const GURL& server,
            const syncer::SyncChainSecret& secret,
            std::optional<NewLogin> login,
            base::DictValue result);
  void OnEncryptorForSave(const std::string& callback_id,
                          const GURL& server,
                          const syncer::SyncChainSecret& secret,
                          std::optional<NewLogin> login,
                          base::DictValue result,
                          scoped_refptr<os_crypt_async::Encryptor> encryptor);
  void OnEncryptorForCode(const std::string& callback_id,
                          scoped_refptr<os_crypt_async::Encryptor> encryptor);

  // Runs `next` with this device's chain secret, or rejects `callback_id`.
  void WithSecret(
      const std::string& callback_id,
      base::OnceCallback<void(const syncer::SyncChainSecret&)> next);
  void OnEncryptorForSecret(
      const std::string& callback_id,
      base::OnceCallback<void(const syncer::SyncChainSecret&)> next,
      scoped_refptr<os_crypt_async::Encryptor> encryptor);

  void OnLoginState(const std::string& callback_id,
                    std::optional<std::string> id,
                    std::optional<std::string> error);
  // Records the sign-in this device set (`login`), or that it removed it.
  void OnLoginChanged(const std::string& callback_id,
                      std::optional<NewLogin> login,
                      std::optional<std::string> error);

  AlerseSyncLoginClient& login_client();

  syncer::SyncService* GetSyncService() const;

  const raw_ptr<Profile> profile_;
  std::unique_ptr<network::SimpleURLLoader> health_check_;
  std::unique_ptr<AlerseSyncLoginClient> login_client_;
  // The pairing in progress, on either device.
  std::unique_ptr<syncer::SyncPairing> pairing_;
  std::optional<syncer::SyncPairingCode> pairing_code_;
  GURL pairing_server_;
  std::optional<syncer::SyncChainSecret> pairing_secret_;
  std::string pairing_name_;
  // Set when the setup changed since the browser started.
  bool restart_needed_ = false;
  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_observation_{this};
  base::WeakPtrFactory<AlerseSyncHandler> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_ALERSE_SYNC_ALERSE_SYNC_HANDLER_H_
