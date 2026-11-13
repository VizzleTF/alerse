// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/alerse_sync/alerse_sync_api_client.h"

#include <algorithm>
#include <utility>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
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

constexpr net::NetworkTrafficAnnotationTag kAccountAnnotation =
    net::DefineNetworkTrafficAnnotation("alerse_sync_account", R"(
      semantics {
        sender: "Alerse Sync"
        description:
          "Creates the user's sync account, signs this device in to it, or "
          "lists, removes devices of it or deletes it. The browser sends "
          "values derived from the 24 words and the password, never the "
          "words, the password or the account key."
        trigger: "The user sets up sync, or manages its devices or account on "
                 "the Sync page."
        data: "A key derived from the words and the password, the account "
              "key encrypted on the device, this device's public key and "
              "name."
        destination: OTHER
        destination_other: "The sync server the user picked."
      }
      policy {
        cookies_allowed: NO
        setting: "Not started unless the user sets up sync."
        policy_exception_justification: "Not implemented."
      })");

constexpr char kNoAnswer[] =
    "The server does not answer. Check the address and that the server is "
    "running.";
constexpr char kNoSupport[] =
    "This sync server is too old for accounts with the 24 words and a "
    "password. Update the server.";
constexpr char kWrongPassword[] = "Wrong words or password.";

std::string TooMany(base::TimeDelta wait) {
  const int64_t minutes = std::max<int64_t>(1, wait.InMinutes() + 1);
  return "Too many attempts. Try again in " + base::NumberToString(minutes) +
         (minutes == 1 ? " minute." : " minutes.");
}

using Keys = std::optional<syncer::SyncAccountKeys>;

Keys Derive(syncer::SyncChainSecret words,
            std::string password,
            std::vector<uint8_t> salt,
            syncer::SyncKdf kdf) {
  return syncer::DeriveSyncAccountKeys(words, password, salt, kdf);
}

void PostDerive(const syncer::SyncChainSecret& words,
                const std::string& password,
                const std::vector<uint8_t>& salt,
                const syncer::SyncKdf& kdf,
                base::OnceCallback<void(Keys)> reply) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&Derive, words, password, salt, kdf), std::move(reply));
}

base::DictValue KdfToDict(const syncer::SyncKdf& kdf) {
  base::DictValue dict;
  dict.Set("name", "scrypt");
  dict.Set("n", static_cast<int>(kdf.n));
  dict.Set("r", static_cast<int>(kdf.r));
  dict.Set("p", static_cast<int>(kdf.p));
  return dict;
}

std::optional<syncer::SyncKdf> KdfFromDict(const base::DictValue* dict) {
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
  return syncer::SyncKdf{.n = static_cast<uint64_t>(*n),
                         .r = static_cast<uint64_t>(*r),
                         .p = static_cast<uint64_t>(*p)};
}

std::optional<std::vector<uint8_t>> Base64Bytes(const base::DictValue& dict,
                                                const char* key) {
  const std::string* value = dict.FindString(key);
  if (!value) {
    return std::nullopt;
  }
  return base::Base64Decode(*value);
}

base::DictValue DeviceInfo(const syncer::SyncChainSecret& seed,
                           const std::string& name) {
  base::DictValue device;
  device.Set("public_key", base::Base64Encode(
                               syncer::AlerseSyncAccount::DevicePublicKey(seed)));
  device.Set("name", name.empty() ? std::string("Alerse") : name.substr(0, 64));
  return device;
}

syncer::SyncChainSecret RandomSecret() {
  syncer::SyncChainSecret secret;
  base::RandBytes(secret);
  return secret;
}

}  // namespace

AlerseSyncApiClient::Response::Response() = default;
AlerseSyncApiClient::Response::Response(Response&&) = default;
AlerseSyncApiClient::Response& AlerseSyncApiClient::Response::operator=(
    Response&&) = default;
AlerseSyncApiClient::Response::~Response() = default;

// static
std::string AlerseSyncApiClient::ErrorMessage(const Response& response) {
  switch (response.status) {
    case 0:
      return kNoAnswer;
    case net::HTTP_UNAUTHORIZED:
      return kWrongPassword;
    case net::HTTP_TOO_MANY_REQUESTS:
      return TooMany(response.retry_after);
    case net::HTTP_NOT_FOUND:
    case net::HTTP_METHOD_NOT_ALLOWED:
      return kNoSupport;
    case net::HTTP_SERVICE_UNAVAILABLE:
      return "The server is full or busy. Try again later.";
    default:
      return "The server refused the request (HTTP " +
             base::NumberToString(response.status) + ").";
  }
}

AlerseSyncApiClient::AlerseSyncApiClient(
    scoped_refptr<network::SharedURLLoaderFactory> factory)
    : factory_(std::move(factory)) {}

AlerseSyncApiClient::~AlerseSyncApiClient() = default;

void AlerseSyncApiClient::Cancel() {
  loaders_.clear();
  weak_factory_.InvalidateWeakPtrs();
}

void AlerseSyncApiClient::Send(const GURL& server,
                               const std::string& method,
                               const std::string& path,
                               const std::string& token,
                               std::optional<base::DictValue> body,
                               Responded done) {
  std::string base = server.spec();
  if (!base.ends_with('/')) {
    base += '/';
  }
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL(base + "v3/" + path);
  request->method = method;
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  if (!token.empty()) {
    request->headers.SetHeader("Authorization", "Bearer " + token);
  }
  auto loader =
      network::SimpleURLLoader::Create(std::move(request), kAccountAnnotation);
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
      base::BindOnce(&AlerseSyncApiClient::OnSent, weak_factory_.GetWeakPtr(),
                     it, std::move(done)),
      kMaxBody);
}

void AlerseSyncApiClient::OnSent(
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

void AlerseSyncApiClient::CreateAccount(const GURL& server,
                                        const std::string& password,
                                        const std::string& device_name,
                                        SignedIn done) {
  Setup setup;
  setup.words = RandomSecret();
  setup.account_key = RandomSecret();
  setup.device_seed = RandomSecret();
  std::vector<uint8_t> salt = syncer::NewSyncSalt();
  PostDerive(
      setup.words, password, salt, syncer::SyncKdf(),
      base::BindOnce(
          [](base::WeakPtr<AlerseSyncApiClient> self, GURL server, Setup setup,
             std::vector<uint8_t> salt, std::string device_name,
             SignedIn done, Keys keys) {
            if (!self) {
              return;
            }
            if (!keys) {
              std::move(done).Run(std::nullopt,
                                  "Alerse could not derive keys from the "
                                  "password.");
              return;
            }
            base::DictValue body;
            body.Set("id", keys->id);
            body.Set("scheme", syncer::kSyncAccountScheme);
            body.Set("salt", base::Base64Encode(salt));
            body.Set("kdf", KdfToDict(syncer::SyncKdf()));
            body.Set("auth", base::Base64Encode(keys->auth));
            body.Set("wrapped",
                     base::Base64Encode(
                         syncer::WrapSyncAccountKey(*keys, setup.account_key)));
            body.Set("device", DeviceInfo(setup.device_seed, device_name));
            self->Send(
                server, "POST", "account", std::string(), std::move(body),
                base::BindOnce(
                    [](Setup setup, SignedIn done, Response response) {
                      const std::string* account =
                          response.json ? response.json->FindString("account")
                                        : nullptr;
                      if (response.status != net::HTTP_CREATED || !account) {
                        std::move(done).Run(
                            std::nullopt,
                            response.status == net::HTTP_CONFLICT
                                ? "These words already have an account."
                                : ErrorMessage(response));
                        return;
                      }
                      setup.account_id = *account;
                      std::move(done).Run(std::move(setup), std::nullopt);
                    },
                    std::move(setup), std::move(done)));
          },
          weak_factory_.GetWeakPtr(), server, setup, salt, device_name,
          std::move(done)));
}

void AlerseSyncApiClient::DeriveKeys(const GURL& server,
                                     const syncer::SyncChainSecret& words,
                                     const std::string& password,
                                     Keyed next) {
  base::DictValue body;
  body.Set("id", syncer::SyncAccountId(words));
  Send(server, "POST", "unlock/params", std::string(), std::move(body),
       base::BindOnce(&AlerseSyncApiClient::OnParams,
                      weak_factory_.GetWeakPtr(), words, password,
                      std::move(next)));
}

void AlerseSyncApiClient::OnParams(const syncer::SyncChainSecret& words,
                                   const std::string& password,
                                   Keyed next,
                                   Response response) {
  if (response.status != net::HTTP_OK || !response.json) {
    std::move(next).Run(std::nullopt, ErrorMessage(response));
    return;
  }
  const std::string* scheme = response.json->FindString("scheme");
  std::optional<std::vector<uint8_t>> salt = Base64Bytes(*response.json, "salt");
  std::optional<syncer::SyncKdf> kdf =
      KdfFromDict(response.json->FindDict("kdf"));
  if (!scheme || *scheme != syncer::kSyncAccountScheme || !salt || !kdf) {
    std::move(next).Run(std::nullopt,
                        "The server answered with an unknown kind of "
                        "account.");
    return;
  }
  // A server that asks for weak parameters gets no key that is cheap to
  // attack.
  if (!syncer::IsAcceptableSyncKdf(*kdf)) {
    std::move(next).Run(std::nullopt,
                        "The server asked for weaker password protection "
                        "than Alerse allows.");
    return;
  }
  PostDerive(words, password, *salt, *kdf,
             base::BindOnce(
                 [](Keyed next, Keys keys) {
                   std::optional<std::string> error;
                   if (!keys) {
                     error = "Alerse could not derive keys from the password.";
                   }
                   std::move(next).Run(std::move(keys), std::move(error));
                 },
                 std::move(next)));
}

void AlerseSyncApiClient::SignIn(const GURL& server,
                                 const syncer::SyncChainSecret& words,
                                 const std::string& password,
                                 const std::string& device_name,
                                 SignedIn done) {
  DeriveKeys(
      server, words, password,
      base::BindOnce(
          [](base::WeakPtr<AlerseSyncApiClient> self, GURL server,
             syncer::SyncChainSecret words, std::string device_name,
             SignedIn done, Keys keys, std::optional<std::string> error) {
            if (!self) {
              return;
            }
            if (!keys) {
              std::move(done).Run(std::nullopt, std::move(error));
              return;
            }
            Setup setup;
            setup.words = words;
            setup.device_seed = RandomSecret();
            base::DictValue body;
            body.Set("id", keys->id);
            body.Set("auth", base::Base64Encode(keys->auth));
            body.Set("device", DeviceInfo(setup.device_seed, device_name));
            self->Send(
                server, "POST", "unlock", std::string(), std::move(body),
                base::BindOnce(
                    [](Setup setup, syncer::SyncAccountKeys keys,
                       SignedIn done, Response response) {
                      const std::string* account =
                          response.json ? response.json->FindString("account")
                                        : nullptr;
                      std::optional<std::vector<uint8_t>> wrapped =
                          response.json
                              ? Base64Bytes(*response.json, "wrapped")
                              : std::nullopt;
                      if (response.status != net::HTTP_OK || !account ||
                          !wrapped) {
                        std::move(done).Run(
                            std::nullopt,
                            response.status == net::HTTP_CONFLICT
                                ? "The account has too many devices. Remove "
                                  "one on a device that syncs."
                                : ErrorMessage(response));
                        return;
                      }
                      std::optional<syncer::SyncChainSecret> account_key =
                          syncer::UnwrapSyncAccountKey(keys, *wrapped);
                      if (!account_key) {
                        std::move(done).Run(std::nullopt,
                                            "The server sent an account key "
                                            "that does not open.");
                        return;
                      }
                      setup.account_id = *account;
                      setup.account_key = *account_key;
                      std::move(done).Run(std::move(setup), std::nullopt);
                    },
                    std::move(setup), *keys, std::move(done)));
          },
          weak_factory_.GetWeakPtr(), server, words, device_name,
          std::move(done)));
}

void AlerseSyncApiClient::ListDevices(const GURL& server,
                                      const Setup& setup,
                                      Devices done) {
  Send(server, "GET", "devices",
       syncer::AlerseSyncAccount::MakeDeviceToken(setup, base::Time::Now()),
       std::nullopt,
       base::BindOnce(
           [](Devices done, Response response) {
             const base::ListValue* list =
                 response.json ? response.json->FindList("devices") : nullptr;
             if (response.status != net::HTTP_OK || !list) {
               std::move(done).Run(std::nullopt,
                                   response.status == net::HTTP_UNAUTHORIZED
                                       ? "This device was removed from the "
                                         "account."
                                       : ErrorMessage(response));
               return;
             }
             std::vector<Device> devices;
             for (const base::Value& value : *list) {
               const base::DictValue* dict = value.GetIfDict();
               if (!dict) {
                 continue;
               }
               Device device;
               if (const std::string* id = dict->FindString("id")) {
                 device.id = *id;
               }
               if (const std::string* name = dict->FindString("name")) {
                 device.name = *name;
               }
               if (const std::string* created = dict->FindString("created")) {
                 device.created = *created;
               }
               if (const std::string* seen = dict->FindString("last_seen")) {
                 device.last_seen = *seen;
               }
               device.current = dict->FindBool("current").value_or(false);
               devices.push_back(std::move(device));
             }
             std::move(done).Run(std::move(devices), std::nullopt);
           },
           std::move(done)));
}

void AlerseSyncApiClient::DeleteWithAuth(const GURL& server,
                                         const Setup& setup,
                                         const std::string& path,
                                         const std::string& password,
                                         Done done) {
  DeriveKeys(
      server, setup.words, password,
      base::BindOnce(
          [](base::WeakPtr<AlerseSyncApiClient> self, GURL server, Setup setup,
             std::string path, Done done, Keys keys,
             std::optional<std::string> error) {
            if (!self) {
              return;
            }
            if (!keys) {
              std::move(done).Run(std::move(error));
              return;
            }
            base::DictValue body;
            body.Set("auth", base::Base64Encode(keys->auth));
            self->Send(server, "DELETE", path,
                       syncer::AlerseSyncAccount::MakeDeviceToken(
                           setup, base::Time::Now()),
                       std::move(body),
                       base::BindOnce(
                           [](Done done, Response response) {
                             if (response.status == net::HTTP_NO_CONTENT) {
                               std::move(done).Run(std::nullopt);
                             } else if (response.status ==
                                        net::HTTP_CONFLICT) {
                               std::move(done).Run(
                                   "The account's only device cannot be "
                                   "removed. Delete the account instead.");
                             } else {
                               std::move(done).Run(ErrorMessage(response));
                             }
                           },
                           std::move(done)));
          },
          weak_factory_.GetWeakPtr(), server, setup, path, std::move(done)));
}

void AlerseSyncApiClient::RemoveDevice(const GURL& server,
                                       const Setup& setup,
                                       const std::string& id,
                                       const std::string& password,
                                       Done done) {
  DeleteWithAuth(server, setup, "devices/" + id, password, std::move(done));
}

void AlerseSyncApiClient::DeleteAccount(const GURL& server,
                                        const Setup& setup,
                                        const std::string& password,
                                        Done done) {
  DeleteWithAuth(server, setup, "account", password, std::move(done));
}
