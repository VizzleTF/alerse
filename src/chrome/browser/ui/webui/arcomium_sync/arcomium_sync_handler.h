// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ARCOMIUM_SYNC_ARCOMIUM_SYNC_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ARCOMIUM_SYNC_ARCOMIUM_SYNC_HANDLER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "components/sync/service/arcomium_sync_code.h"
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

// Backs chrome://arcomium-sync: starting or joining a sync chain, showing its
// sync code, leaving it. The page calls:
//
//   getState()                 -> state
//   createChain(server)        -> {code}
//   joinChain(server, code)    -> {}
//   showCode()                 -> {code}
//   leaveChain()               -> {}
//   restart()
//
// and gets "state-changed" with the state whenever sync changes.
class ArcomiumSyncHandler : public content::WebUIMessageHandler,
                            public syncer::SyncServiceObserver {
 public:
  explicit ArcomiumSyncHandler(Profile* profile);
  ArcomiumSyncHandler(const ArcomiumSyncHandler&) = delete;
  ArcomiumSyncHandler& operator=(const ArcomiumSyncHandler&) = delete;
  ~ArcomiumSyncHandler() override;

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

  base::DictValue BuildState() const;

  // Checks that `server` answers, then saves the setup with `secret` and
  // resolves `callback_id` with `result`.
  void CheckServerAndSave(const std::string& callback_id,
                          const GURL& server,
                          const syncer::SyncChainSecret& secret,
                          base::DictValue result);
  void OnServerChecked(const std::string& callback_id,
                       const GURL& server,
                       const syncer::SyncChainSecret& secret,
                       base::DictValue result,
                       std::optional<std::string> body);
  void OnEncryptorForSave(const std::string& callback_id,
                          const GURL& server,
                          const syncer::SyncChainSecret& secret,
                          base::DictValue result,
                          scoped_refptr<os_crypt_async::Encryptor> encryptor);
  void OnEncryptorForCode(const std::string& callback_id,
                          scoped_refptr<os_crypt_async::Encryptor> encryptor);

  syncer::SyncService* GetSyncService() const;

  const raw_ptr<Profile> profile_;
  std::unique_ptr<network::SimpleURLLoader> health_check_;
  // Set when the setup changed since the browser started.
  bool restart_needed_ = false;
  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_observation_{this};
  base::WeakPtrFactory<ArcomiumSyncHandler> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_ARCOMIUM_SYNC_ARCOMIUM_SYNC_HANDLER_H_
