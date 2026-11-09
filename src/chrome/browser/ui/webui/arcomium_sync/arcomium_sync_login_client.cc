// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/arcomium_sync/arcomium_sync_login_client.h"

#include <algorithm>
#include <utility>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "components/sync/service/arcomium_sync_account.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

constexpr base::TimeDelta kTimeout = base::Seconds(30);
constexpr size_t kMaxBody = 64 * 1024;

constexpr net::NetworkTrafficAnnotationTag kLoginAnnotation =
    net::DefineNetworkTrafficAnnotation("arcomium_sync_login", R"(
      semantics {
        sender: "Arcomium Sync"
        description:
          "Sets, changes or removes the optional sign-in of the user's sync "
          "chain, or joins a chain with it. The browser sends values derived "
          "from the login name and password, never the name, the password or "
          "the chain's secret."
        trigger: "The user sets up, changes or removes the sign-in, or joins "
                 "a sync chain with a login name and password."
        data: "A hash of the login name, a key derived from the password, "
              "and the chain's secret encrypted on the device."
        destination: OTHER
        destination_other: "The sync server the user set up."
      }
      policy {
        cookies_allowed: NO
        setting: "Not started unless the user sets up sync with a sign-in."
        policy_exception_justification: "Not implemented."
      })");

constexpr char kNoAnswer[] =
    "The server does not answer. Check the address and that the server is "
    "running.";
constexpr char kNoSupport[] =
    "This sync server does not support signing in with a password. Update "
    "the server, or use the sync code.";

std::string TooMany(base::TimeDelta wait) {
  const int64_t minutes = std::max<int64_t>(1, wait.InMinutes() + 1);
  return "Too many attempts. Try again in " + base::NumberToString(minutes) +
         (minutes == 1 ? " minute." : " minutes.");
}

// The error for an answer other than the expected one.
std::string ErrorFor(int status,
                     const std::optional<base::DictValue>& json,
                     base::TimeDelta retry_after) {
  switch (status) {
    case 0:
      return kNoAnswer;
    case net::HTTP_TOO_MANY_REQUESTS:
      return TooMany(retry_after);
    case net::HTTP_NOT_FOUND:
    case net::HTTP_METHOD_NOT_ALLOWED:
      return kNoSupport;
    case net::HTTP_SERVICE_UNAVAILABLE:
      return "The server is full or busy. Try again later.";
    default:
      break;
  }
  std::string message = "The server refused the request (HTTP " +
                        base::NumberToString(status) + ")";
  if (json) {
    if (const std::string* detail = json->FindString("error")) {
      message += ": " + *detail;
    }
  }
  return message + ".";
}

std::optional<syncer::SyncLoginKeys> Derive(std::string id,
                                            std::string password,
                                            std::vector<uint8_t> salt,
                                            syncer::SyncLoginKdf kdf) {
  return syncer::DeriveSyncLoginKeys(id, password, salt, kdf);
}

base::DictValue KdfToDict(const syncer::SyncLoginKdf& kdf) {
  base::DictValue dict;
  dict.Set("name", "scrypt");
  dict.Set("n", static_cast<int>(kdf.n));
  dict.Set("r", static_cast<int>(kdf.r));
  dict.Set("p", static_cast<int>(kdf.p));
  return dict;
}

std::optional<syncer::SyncLoginKdf> KdfFromDict(const base::DictValue* dict) {
  if (!dict) {
    return std::nullopt;
  }
  const std::string* name = dict->FindString("name");
  const std::optional<int> n = dict->FindInt("n");
  const std::optional<int> r = dict->FindInt("r");
  const std::optional<int> p = dict->FindInt("p");
  if (!name || *name != "scrypt" || !n || !r || !p || *n <= 0 || *r <= 0 ||
      *p <= 0) {
    return std::nullopt;
  }
  return syncer::SyncLoginKdf{.n = static_cast<uint64_t>(*n),
                              .r = static_cast<uint64_t>(*r),
                              .p = static_cast<uint64_t>(*p)};
}

void PostDerive(const std::string& id,
                const std::string& password,
                const std::vector<uint8_t>& salt,
                const syncer::SyncLoginKdf& kdf,
                base::OnceCallback<void(std::optional<syncer::SyncLoginKeys>)>
                    reply) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&Derive, id, password, salt, kdf), std::move(reply));
}

}  // namespace

ArcomiumSyncLoginClient::Response::Response() = default;
ArcomiumSyncLoginClient::Response::Response(Response&&) = default;
ArcomiumSyncLoginClient::Response& ArcomiumSyncLoginClient::Response::operator=(
    Response&&) = default;
ArcomiumSyncLoginClient::Response::~Response() = default;

// static
std::string ArcomiumSyncLoginClient::ErrorMessage(const Response& response) {
  return ErrorFor(response.status, response.json, response.retry_after);
}

ArcomiumSyncLoginClient::ArcomiumSyncLoginClient(
    scoped_refptr<network::SharedURLLoaderFactory> factory)
    : factory_(std::move(factory)) {}

ArcomiumSyncLoginClient::~ArcomiumSyncLoginClient() = default;

void ArcomiumSyncLoginClient::Cancel() {
  loaders_.clear();
  weak_factory_.InvalidateWeakPtrs();
}

void ArcomiumSyncLoginClient::Send(
    const GURL& server,
    const std::string& method,
    const std::string& path,
    const std::optional<syncer::SyncChainSecret>& secret,
    std::optional<base::DictValue> body,
    Responded done) {
  std::string base = server.spec();
  if (!base.ends_with('/')) {
    base += '/';
  }
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL(base + path);
  request->method = method;
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  if (secret) {
    request->headers.SetHeader(
        "Authorization",
        "Bearer " +
            syncer::ArcomiumSyncAccount(*secret).MakeToken(base::Time::Now()));
  }
  auto loader =
      network::SimpleURLLoader::Create(std::move(request), kLoginAnnotation);
  if (body) {
    loader->AttachStringForUpload(base::WriteJson(*body).value_or("{}"),
                                  "application/json");
  }
  loader->SetAllowHttpErrorResults(true);
  loader->SetTimeoutDuration(kTimeout);
  loaders_.push_front(std::move(loader));
  auto it = loaders_.begin();
  (*it)->DownloadToString(
      factory_.get(),
      base::BindOnce(&ArcomiumSyncLoginClient::OnSent,
                     weak_factory_.GetWeakPtr(), it, std::move(done)),
      kMaxBody);
}

void ArcomiumSyncLoginClient::OnSent(
    std::list<std::unique_ptr<network::SimpleURLLoader>>::iterator it,
    Responded done,
    std::optional<std::string> body) {
  Response response;
  const network::mojom::URLResponseHead* head = (*it)->ResponseInfo();
  if (head && head->headers) {
    response.status = head->headers->response_code();
    if (std::optional<std::string> retry =
            head->headers->GetNormalizedHeader("Retry-After")) {
      int seconds = 0;
      if (base::StringToInt(*retry, &seconds) && seconds > 0) {
        response.retry_after = base::Seconds(seconds);
      }
    }
  }
  if (body) {
    response.json = base::JSONReader::ReadDict(*body, base::JSON_PARSE_RFC);
  }
  loaders_.erase(it);
  std::move(done).Run(std::move(response));
}

void ArcomiumSyncLoginClient::SetLogin(const GURL& server,
                                       const syncer::SyncChainSecret& secret,
                                       const std::string& login,
                                       const std::string& password,
                                       Done done) {
  const std::string id = syncer::SyncLoginId(login);
  std::vector<uint8_t> salt = syncer::NewSyncLoginSalt();
  PostDerive(id, password, salt, syncer::SyncLoginKdf(),
             base::BindOnce(&ArcomiumSyncLoginClient::OnKeysForSet,
                            weak_factory_.GetWeakPtr(), server, secret, id,
                            salt, std::move(done)));
}

void ArcomiumSyncLoginClient::OnKeysForSet(
    const GURL& server,
    const syncer::SyncChainSecret& secret,
    const std::string& id,
    std::vector<uint8_t> salt,
    Done done,
    std::optional<syncer::SyncLoginKeys> keys) {
  if (!keys) {
    std::move(done).Run("Alerse could not derive keys from the password.");
    return;
  }
  base::DictValue body;
  body.Set("id", id);
  body.Set("scheme", syncer::kSyncLoginScheme);
  body.Set("salt", base::Base64Encode(salt));
  body.Set("kdf", KdfToDict(syncer::SyncLoginKdf()));
  body.Set("auth", base::Base64Encode(keys->auth));
  body.Set("secret",
           base::Base64Encode(syncer::WrapSyncChainSecret(*keys, id, secret)));
  Send(server, "PUT", "login", secret, std::move(body),
       base::BindOnce(
           [](Done done, Response response) {
             if (response.status == net::HTTP_NO_CONTENT) {
               std::move(done).Run(std::nullopt);
             } else if (response.status == net::HTTP_CONFLICT) {
               std::move(done).Run(
                   "This email or login name is already used by another sync "
                   "chain. Choose another one.");
             } else {
               std::move(done).Run(ErrorFor(response.status, response.json,
                                            response.retry_after));
             }
           },
           std::move(done)));
}

void ArcomiumSyncLoginClient::RemoveLogin(const GURL& server,
                                          const syncer::SyncChainSecret& secret,
                                          Done done) {
  Send(server, "DELETE", "login", secret, std::nullopt,
       base::BindOnce(
           [](Done done, Response response) {
             if (response.status == net::HTTP_NO_CONTENT) {
               std::move(done).Run(std::nullopt);
             } else {
               std::move(done).Run(ErrorFor(response.status, response.json,
                                            response.retry_after));
             }
           },
           std::move(done)));
}

void ArcomiumSyncLoginClient::GetLogin(const GURL& server,
                                       const syncer::SyncChainSecret& secret,
                                       Status done) {
  Send(server, "GET", "login", secret, std::nullopt,
       base::BindOnce(
           [](Status done, Response response) {
             const std::string* id =
                 response.json ? response.json->FindString("id") : nullptr;
             if (response.status == net::HTTP_OK && id) {
               std::move(done).Run(*id, std::nullopt);
             } else if (response.status == net::HTTP_NOT_FOUND &&
                        response.json) {
               // The server knows logins, and the chain has none.
               std::move(done).Run("none", std::nullopt);
             } else {
               std::move(done).Run(std::nullopt,
                                   ErrorFor(response.status, response.json,
                                            response.retry_after));
             }
           },
           std::move(done)));
}

void ArcomiumSyncLoginClient::Unlock(const GURL& server,
                                     const std::string& login,
                                     const std::string& password,
                                     Unlocked done) {
  base::DictValue body;
  body.Set("id", syncer::SyncLoginId(login));
  Send(server, "POST", "login/params", std::nullopt, std::move(body),
       base::BindOnce(&ArcomiumSyncLoginClient::OnParams,
                      weak_factory_.GetWeakPtr(), server, login, password,
                      std::move(done)));
}

void ArcomiumSyncLoginClient::OnParams(const GURL& server,
                                       const std::string& login,
                                       const std::string& password,
                                       Unlocked done,
                                       Response response) {
  if (response.status != net::HTTP_OK || !response.json) {
    std::move(done).Run(std::nullopt, ErrorFor(response.status, response.json,
                                               response.retry_after));
    return;
  }
  const std::string* scheme = response.json->FindString("scheme");
  const std::string* salt64 = response.json->FindString("salt");
  const std::optional<syncer::SyncLoginKdf> kdf =
      KdfFromDict(response.json->FindDict("kdf"));
  std::optional<std::vector<uint8_t>> salt =
      salt64 ? base::Base64Decode(*salt64) : std::nullopt;
  if (!scheme || *scheme != syncer::kSyncLoginScheme || !salt || !kdf) {
    std::move(done).Run(std::nullopt,
                        "This sync server uses a sign-in this version of "
                        "Alerse does not know. Update Alerse, or use the "
                        "sync code.");
    return;
  }
  // A server that asks for weaker parameters than this device's would get
  // an authentication key that is cheap to attack.
  if (!syncer::IsAcceptableSyncLoginKdf(*kdf)) {
    std::move(done).Run(std::nullopt,
                        "The server asks for a weaker key derivation than "
                        "Alerse allows, so the password was not sent. Use "
                        "the sync code.");
    return;
  }
  const std::string id = syncer::SyncLoginId(login);
  const bool rewrap = kdf->n < syncer::SyncLoginKdf().n;
  PostDerive(id, password, *salt, *kdf,
             base::BindOnce(&ArcomiumSyncLoginClient::OnKeysForUnlock,
                            weak_factory_.GetWeakPtr(), server, login,
                            password, id, rewrap, std::move(done)));
}

void ArcomiumSyncLoginClient::OnKeysForUnlock(
    const GURL& server,
    const std::string& login,
    const std::string& password,
    const std::string& id,
    bool rewrap,
    Unlocked done,
    std::optional<syncer::SyncLoginKeys> keys) {
  if (!keys) {
    std::move(done).Run(std::nullopt,
                        "Alerse could not derive keys from the password.");
    return;
  }
  base::DictValue body;
  body.Set("id", id);
  body.Set("auth", base::Base64Encode(keys->auth));
  Send(server, "POST", "login/unlock", std::nullopt, std::move(body),
       base::BindOnce(&ArcomiumSyncLoginClient::OnUnlocked,
                      weak_factory_.GetWeakPtr(), server, login, password, id,
                      *keys, rewrap, std::move(done)));
}

void ArcomiumSyncLoginClient::OnUnlocked(const GURL& server,
                                         const std::string& login,
                                         const std::string& password,
                                         const std::string& id,
                                         const syncer::SyncLoginKeys& keys,
                                         bool rewrap,
                                         Unlocked done,
                                         Response response) {
  if (response.status == net::HTTP_UNAUTHORIZED) {
    std::move(done).Run(std::nullopt,
                        "Wrong email, login name or password.");
    return;
  }
  if (response.status != net::HTTP_OK || !response.json) {
    std::move(done).Run(std::nullopt, ErrorFor(response.status, response.json,
                                               response.retry_after));
    return;
  }
  const std::string* wrapped64 = response.json->FindString("secret");
  const std::optional<std::vector<uint8_t>> wrapped =
      wrapped64 ? base::Base64Decode(*wrapped64) : std::nullopt;
  const std::optional<syncer::SyncChainSecret> secret =
      wrapped ? syncer::UnwrapSyncChainSecret(keys, id, *wrapped)
              : std::nullopt;
  if (!secret) {
    std::move(done).Run(std::nullopt,
                        "The server's answer does not open with this "
                        "password. Use the sync code.");
    return;
  }
  if (!rewrap) {
    std::move(done).Run(secret, std::nullopt);
    return;
  }
  // Stored with older, weaker parameters: wrap it again with today's. The
  // device joins either way.
  SetLogin(server, *secret, login, password,
           base::BindOnce(
               [](syncer::SyncChainSecret secret, Unlocked done,
                  std::optional<std::string> error) {
                 std::move(done).Run(secret, std::nullopt);
               },
               *secret, std::move(done)));
}
