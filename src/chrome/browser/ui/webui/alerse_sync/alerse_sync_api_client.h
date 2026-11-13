// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ALERSE_SYNC_ALERSE_SYNC_API_CLIENT_H_
#define CHROME_BROWSER_UI_WEBUI_ALERSE_SYNC_ALERSE_SYNC_API_CLIENT_H_

#include <list>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/sync/service/alerse_sync_account.h"
#include "components/sync/service/alerse_sync_sk.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

// Talks to the /v3 account endpoints of an alerse-sync server (scheme sk-v1,
// alerse-sync's docs/protocol.md): creates an account, signs this device in,
// lists and removes devices, deletes the account. Key derivation runs on the
// thread pool. Every operation ends with a callback that carries an error
// message for the user, or none.
class AlerseSyncApiClient {
 public:
  using Setup = syncer::AlerseSyncAccount::Setup;
  // An error for the user, or nullopt on success.
  using Done = base::OnceCallback<void(std::optional<std::string> error)>;
  // The setup this device keeps, or an error.
  using SignedIn = base::OnceCallback<void(std::optional<Setup> setup,
                                           std::optional<std::string> error)>;
  struct Device {
    std::string id;  // the public key, hex
    std::string name;
    // RFC 3339, as the server sends them; last_seen may be empty.
    std::string created;
    std::string last_seen;
    bool current = false;
  };
  using Devices = base::OnceCallback<void(std::optional<std::vector<Device>>,
                                          std::optional<std::string> error)>;

  explicit AlerseSyncApiClient(
      scoped_refptr<network::SharedURLLoaderFactory> factory);
  AlerseSyncApiClient(const AlerseSyncApiClient&) = delete;
  AlerseSyncApiClient& operator=(const AlerseSyncApiClient&) = delete;
  ~AlerseSyncApiClient();

  // Makes a new account with new words, a new account key and a key for this
  // device, which joins it as `device_name`.
  void CreateAccount(const GURL& server,
                     const std::string& password,
                     const std::string& device_name,
                     SignedIn done);

  // Signs this device in to the account of `words` with a new device key.
  void SignIn(const GURL& server,
              const syncer::SyncChainSecret& words,
              const std::string& password,
              const std::string& device_name,
              SignedIn done);

  void ListDevices(const GURL& server, const Setup& setup, Devices done);

  // Removes the device `id` of the account; takes the password again.
  void RemoveDevice(const GURL& server,
                    const Setup& setup,
                    const std::string& id,
                    const std::string& password,
                    Done done);

  // Deletes the account with all its data; takes the password again.
  void DeleteAccount(const GURL& server,
                     const Setup& setup,
                     const std::string& password,
                     Done done);

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

  // A message for the user about an answer that was not the expected one.
  static std::string ErrorMessage(const Response& response);

 private:
  // Sends a JSON request to `path` under the server's /v3, with `token` as
  // the bearer if set.
  void Send(const GURL& server,
            const std::string& method,
            const std::string& path,
            const std::string& token,
            std::optional<base::DictValue> body,
            Responded done);
  void OnSent(std::list<std::unique_ptr<network::SimpleURLLoader>>::iterator it,
              Responded done,
              std::optional<std::string> body);

  // Fetches the account's parameters and derives its keys, then runs `next`.
  using Keyed = base::OnceCallback<void(
      std::optional<syncer::SyncAccountKeys> keys,
      std::optional<std::string> error)>;
  void DeriveKeys(const GURL& server,
                  const syncer::SyncChainSecret& words,
                  const std::string& password,
                  Keyed next);
  void OnParams(const syncer::SyncChainSecret& words,
                const std::string& password,
                Keyed next,
                Response response);

  // Sends DELETE `path` with the auth key of `password`.
  void DeleteWithAuth(const GURL& server,
                      const Setup& setup,
                      const std::string& path,
                      const std::string& password,
                      Done done);

  scoped_refptr<network::SharedURLLoaderFactory> factory_;
  std::list<std::unique_ptr<network::SimpleURLLoader>> loaders_;
  base::WeakPtrFactory<AlerseSyncApiClient> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_ALERSE_SYNC_ALERSE_SYNC_API_CLIENT_H_
