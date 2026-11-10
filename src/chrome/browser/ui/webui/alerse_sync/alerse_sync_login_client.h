// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ALERSE_SYNC_ALERSE_SYNC_LOGIN_CLIENT_H_
#define CHROME_BROWSER_UI_WEBUI_ALERSE_SYNC_ALERSE_SYNC_LOGIN_CLIENT_H_

#include <list>
#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/sync/service/alerse_sync_code.h"
#include "components/sync/service/alerse_sync_login.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

// Talks to the /v2/login endpoints of an alerse-sync server: sets, removes
// and reads a chain's sign-in, and fetches a chain's secret with a login name
// and password. Key derivation runs on the thread pool. Every operation ends
// with a callback that carries an error message for the user, or none.
class AlerseSyncLoginClient {
 public:
  // An error for the user, or nullopt on success.
  using Done = base::OnceCallback<void(std::optional<std::string> error)>;
  // The chain's secret, or an error.
  using Unlocked =
      base::OnceCallback<void(std::optional<syncer::SyncChainSecret> secret,
                              std::optional<std::string> error)>;
  // "none", or the server's login id of the chain; empty with an error.
  using Status = base::OnceCallback<void(std::optional<std::string> id,
                                         std::optional<std::string> error)>;

  explicit AlerseSyncLoginClient(
      scoped_refptr<network::SharedURLLoaderFactory> factory);
  AlerseSyncLoginClient(const AlerseSyncLoginClient&) = delete;
  AlerseSyncLoginClient& operator=(const AlerseSyncLoginClient&) = delete;
  ~AlerseSyncLoginClient();

  // Sets the sign-in of the chain of `secret`: wraps the secret with keys
  // from `login` (normalized) and `password`, and stores it on the server,
  // replacing the chain's earlier sign-in.
  void SetLogin(const GURL& server,
                const syncer::SyncChainSecret& secret,
                const std::string& login,
                const std::string& password,
                Done done);

  void RemoveLogin(const GURL& server,
                   const syncer::SyncChainSecret& secret,
                   Done done);

  // The server's login id of the chain, or "none".
  void GetLogin(const GURL& server,
                const syncer::SyncChainSecret& secret,
                Status done);

  // Fetches and opens the secret of the chain that `login` signs in to.
  // If the server holds it with weaker parameters than this device uses now,
  // it is wrapped again with the current ones.
  void Unlock(const GURL& server,
              const std::string& login,
              const std::string& password,
              Unlocked done);

  // Cancels everything in progress; no callback runs.
  void Cancel();

  struct Response {
    Response();
    Response(Response&&);
    Response& operator=(Response&&);
    ~Response();
    int status = 0;  // 0: no answer
    std::optional<base::DictValue> json;
    base::TimeDelta retry_after;
  };
  using Responded = base::OnceCallback<void(Response)>;

  // Sends a JSON request to `path` under the server's /v2, signed with the
  // chain's token if `secret` is set.
  void Send(const GURL& server,
            const std::string& method,
            const std::string& path,
            const std::optional<syncer::SyncChainSecret>& secret,
            std::optional<base::DictValue> body,
            Responded done);

  // A message for the user about an answer that was not the expected one.
  static std::string ErrorMessage(const Response& response);

 private:
  void OnSent(std::list<std::unique_ptr<network::SimpleURLLoader>>::iterator it,
              Responded done,
              std::optional<std::string> body);

  void OnKeysForSet(const GURL& server,
                    const syncer::SyncChainSecret& secret,
                    const std::string& id,
                    std::vector<uint8_t> salt,
                    Done done,
                    std::optional<syncer::SyncLoginKeys> keys);
  void OnParams(const GURL& server,
                const std::string& login,
                const std::string& password,
                Unlocked done,
                Response response);
  void OnKeysForUnlock(const GURL& server,
                       const std::string& login,
                       const std::string& password,
                       const std::string& id,
                       bool rewrap,
                       Unlocked done,
                       std::optional<syncer::SyncLoginKeys> keys);
  void OnUnlocked(const GURL& server,
                  const std::string& login,
                  const std::string& password,
                  const std::string& id,
                  const syncer::SyncLoginKeys& keys,
                  bool rewrap,
                  Unlocked done,
                  Response response);

  scoped_refptr<network::SharedURLLoaderFactory> factory_;
  std::list<std::unique_ptr<network::SimpleURLLoader>> loaders_;
  base::WeakPtrFactory<AlerseSyncLoginClient> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_ALERSE_SYNC_ALERSE_SYNC_LOGIN_CLIENT_H_
