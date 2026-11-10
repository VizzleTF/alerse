// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/alerse_sync/alerse_sync_handler.h"

#include <utility>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/qr_code_generator/qr_code_generator.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/prefs/pref_service.h"
#include "components/sync/service/alerse_sync_account.h"
#include "components/sync/service/alerse_sync_login.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync_device_info/local_device_info_util.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/url_constants.h"

namespace {

constexpr base::TimeDelta kHealthCheckTimeout = base::Seconds(10);

constexpr net::NetworkTrafficAnnotationTag kHealthCheckAnnotation =
    net::DefineNetworkTrafficAnnotation("alerse_sync_server_check", R"(
      semantics {
        sender: "Alerse Sync"
        description:
          "Checks that the sync server the user typed answers before the "
          "browser starts syncing with it."
        trigger: "The user starts or joins a sync chain."
        data: "None."
        destination: OTHER
        destination_other: "The sync server the user typed."
      }
      policy {
        cookies_allowed: NO
        setting: "Not started unless the user sets up sync."
        policy_exception_justification: "Not implemented."
      })");

// Reads a server address as people type it: "https://sync.example.com" means
// its protocol endpoint, "https://sync.example.com/v2". Only https, but for a
// server on this computer: the data is end-to-end encrypted, its metadata and
// the access token are not.
GURL NormalizeServerURL(const std::string& typed) {
  GURL url(typed);
  if (!url.is_valid() || url.host().empty() ||
      !(url.SchemeIs(url::kHttpsScheme) ||
        (url.SchemeIs(url::kHttpScheme) && net::IsLocalhost(url)))) {
    return GURL();
  }
  GURL::Replacements clean;
  clean.ClearQuery();
  clean.ClearRef();
  if (url.path() == "/" || url.path().empty()) {
    clean.SetPathStr("/v2");
  }
  return url.ReplaceComponents(clean);
}

// The JavaScript time of `time`, or 0 for none.
double ToJsTime(base::Time time) {
  return time.is_null() ? 0 : time.InMillisecondsFSinceUnixEpoch();
}

}  // namespace

AlerseSyncHandler::AlerseSyncHandler(Profile* profile)
    : profile_(profile) {}

AlerseSyncHandler::~AlerseSyncHandler() = default;

void AlerseSyncHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getState", base::BindRepeating(&AlerseSyncHandler::HandleGetState,
                                      base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "createChain",
      base::BindRepeating(&AlerseSyncHandler::HandleCreateChain,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "joinChain", base::BindRepeating(&AlerseSyncHandler::HandleJoinChain,
                                       base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "showCode", base::BindRepeating(&AlerseSyncHandler::HandleShowCode,
                                      base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "leaveChain",
      base::BindRepeating(&AlerseSyncHandler::HandleLeaveChain,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "joinWithLogin",
      base::BindRepeating(&AlerseSyncHandler::HandleJoinWithLogin,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getLoginState",
      base::BindRepeating(&AlerseSyncHandler::HandleGetLoginState,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setLogin", base::BindRepeating(&AlerseSyncHandler::HandleSetLogin,
                                      base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "removeLogin",
      base::BindRepeating(&AlerseSyncHandler::HandleRemoveLogin,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "checkPassword",
      base::BindRepeating(&AlerseSyncHandler::HandleCheckPassword,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "startPairing",
      base::BindRepeating(&AlerseSyncHandler::HandleStartPairing,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "pairingStatus",
      base::BindRepeating(&AlerseSyncHandler::HandlePairingStatus,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "approvePairing",
      base::BindRepeating(&AlerseSyncHandler::HandleApprovePairing,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "cancelPairing",
      base::BindRepeating(&AlerseSyncHandler::HandleCancelPairing,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "joinWithPairing",
      base::BindRepeating(&AlerseSyncHandler::HandleJoinWithPairing,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "pairingJoinStatus",
      base::BindRepeating(&AlerseSyncHandler::HandlePairingJoinStatus,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "restart", base::BindRepeating(&AlerseSyncHandler::HandleRestart,
                                     base::Unretained(this)));
}

void AlerseSyncHandler::OnJavascriptAllowed() {
  if (syncer::SyncService* sync = GetSyncService()) {
    sync_observation_.Observe(sync);
  }
}

void AlerseSyncHandler::OnJavascriptDisallowed() {
  sync_observation_.Reset();
  health_check_.reset();
  if (login_client_) {
    login_client_->Cancel();
  }
  weak_factory_.InvalidateWeakPtrs();
}

void AlerseSyncHandler::OnStateChanged(syncer::SyncService* sync) {
  FireWebUIListener("state-changed", BuildState());
}

void AlerseSyncHandler::OnSyncShutdown(syncer::SyncService* sync) {
  sync_observation_.Reset();
}

syncer::SyncService* AlerseSyncHandler::GetSyncService() const {
  return SyncServiceFactory::GetForProfile(profile_);
}

base::DictValue AlerseSyncHandler::BuildState() const {
  const GURL server =
      syncer::AlerseSyncAccount::GetServerURL(*profile_->GetPrefs());
  const syncer::SyncService* sync = GetSyncService();

  base::DictValue state;
  state.Set("server", server.is_valid() ? server.spec() : std::string());
  // The sync service exists only for a profile set up when the browser
  // started; a change since then takes a restart.
  state.Set("restartNeeded",
            restart_needed_ || server.is_valid() != (sync != nullptr));

  std::string status = "off";
  if (sync && server.is_valid()) {
    using TransportState = syncer::SyncService::TransportState;
    switch (sync->GetTransportState()) {
      case TransportState::DISABLED:
      case TransportState::PAUSED:
        status = "stopped";
        break;
      case TransportState::START_DEFERRED:
      case TransportState::INITIALIZING:
      case TransportState::PENDING_DESIRED_CONFIGURATION:
      case TransportState::CONFIGURING:
        status = "starting";
        break;
      case TransportState::ACTIVE:
        status = "active";
        break;
    }
    if (sync->GetUserSettings()->IsPassphraseRequired()) {
      status = "wrongCode";
    } else if (sync->GetAuthError().state() !=
               GoogleServiceAuthError::NONE) {
      status = "refused";
    } else if (sync->HasUnrecoverableError()) {
      status = "error";
    }
    state.Set("lastSynced", ToJsTime(sync->GetLastSyncedTimeForDebugging()));
  }
  state.Set("status", status);
  return state;
}

void AlerseSyncHandler::HandleGetState(const base::ListValue& args) {
  AllowJavascript();
  ResolveJavascriptCallback(args[0], BuildState());
}

std::optional<AlerseSyncHandler::NewLogin> AlerseSyncHandler::ReadNewLogin(
    const std::string& callback_id,
    const base::Value& value) {
  const base::DictValue* dict = value.GetIfDict();
  const std::string* login = dict ? dict->FindString("login") : nullptr;
  const std::string* password = dict ? dict->FindString("password") : nullptr;
  const std::string* confirm = dict ? dict->FindString("confirm") : nullptr;
  NewLogin result;
  result.login = login ? syncer::NormalizeSyncLogin(*login) : std::string();
  if (result.login.empty()) {
    RejectJavascriptCallback(callback_id,
                             base::Value("Enter an email address or a login "
                                         "name."));
    return std::nullopt;
  }
  if (!password || !confirm || *password != *confirm) {
    RejectJavascriptCallback(callback_id,
                             base::Value("The passwords do not match."));
    return std::nullopt;
  }
  switch (syncer::CheckSyncPassword(*password, result.login)) {
    case syncer::SyncPasswordProblem::kNone:
      break;
    case syncer::SyncPasswordProblem::kTooShort:
      RejectJavascriptCallback(
          callback_id, base::Value("Use a password of at least 15 characters. "
                                   "A few unrelated words work well."));
      return std::nullopt;
    case syncer::SyncPasswordProblem::kTooLong:
      RejectJavascriptCallback(
          callback_id, base::Value("Use a password of at most 256 "
                                   "characters."));
      return std::nullopt;
    case syncer::SyncPasswordProblem::kCommon:
      RejectJavascriptCallback(
          callback_id, base::Value("That password is too common or too easy "
                                   "to guess. Choose another one."));
      return std::nullopt;
    case syncer::SyncPasswordProblem::kContainsLogin:
      RejectJavascriptCallback(
          callback_id, base::Value("Do not use your login name, Alerse or "
                                   "Alerse in the password."));
      return std::nullopt;
  }
  result.password = *password;
  return result;
}

void AlerseSyncHandler::HandleCreateChain(const base::ListValue& args) {
  AllowJavascript();
  const std::string& callback_id = args[0].GetString();
  const GURL server = NormalizeServerURL(args[1].GetString());
  if (!server.is_valid()) {
    RejectJavascriptCallback(callback_id,
                             base::Value("Enter the server's https address, such "
                                         "as https://sync.example.com."));
    return;
  }
  std::optional<NewLogin> login;
  if (args.size() > 2 && args[2].is_dict()) {
    login = ReadNewLogin(callback_id, args[2]);
    if (!login) {
      return;
    }
  }
  const syncer::SyncChainSecret secret = syncer::GenerateSyncChainSecret();
  base::DictValue result;
  result.Set("code", syncer::EncodeSyncCode(secret));
  if (!login) {
    CheckServer(callback_id, server,
                base::BindOnce(&AlerseSyncHandler::Save,
                               weak_factory_.GetWeakPtr(), callback_id, server,
                               secret, std::nullopt, std::move(result)));
    return;
  }
  // The sign-in is stored on the server before the setup is saved here, so
  // a sign-in the server refuses leaves nothing behind on this device.
  CheckServer(
      callback_id, server,
      base::BindOnce(
          [](base::WeakPtr<AlerseSyncHandler> self,
             const std::string& callback_id, const GURL& server,
             const syncer::SyncChainSecret& secret, NewLogin login,
             base::DictValue result) {
            if (!self) {
              return;
            }
            const std::string name = login.login;
            const std::string password = login.password;
            self->login_client().SetLogin(
                server, secret, name, password,
                base::BindOnce(
                    [](base::WeakPtr<AlerseSyncHandler> self,
                       const std::string& callback_id, const GURL& server,
                       const syncer::SyncChainSecret& secret, NewLogin login,
                       base::DictValue result,
                       std::optional<std::string> error) {
                      if (!self) {
                        return;
                      }
                      if (error) {
                        self->RejectJavascriptCallback(callback_id,
                                                       base::Value(*error));
                        return;
                      }
                      self->Save(callback_id, server, secret, std::move(login),
                                 std::move(result));
                    },
                    self, callback_id, server, secret, std::move(login),
                    std::move(result)));
          },
          weak_factory_.GetWeakPtr(), callback_id, server, secret,
          std::move(*login), std::move(result)));
}

void AlerseSyncHandler::HandleJoinChain(const base::ListValue& args) {
  AllowJavascript();
  const std::string& callback_id = args[0].GetString();
  const GURL server = NormalizeServerURL(args[1].GetString());
  if (!server.is_valid()) {
    RejectJavascriptCallback(callback_id,
                             base::Value("Enter the server's https address, such "
                                         "as https://sync.example.com."));
    return;
  }
  const std::optional<syncer::SyncChainSecret> secret =
      syncer::DecodeSyncCode(args[2].GetString());
  if (!secret) {
    RejectJavascriptCallback(
        callback_id,
        base::Value("That is not a sync code. Check that all 24 words are "
                    "there, in order."));
    return;
  }
  CheckServer(callback_id, server,
              base::BindOnce(&AlerseSyncHandler::Save,
                             weak_factory_.GetWeakPtr(), callback_id, server,
                             *secret, std::nullopt, base::DictValue()));
}

void AlerseSyncHandler::HandleJoinWithLogin(const base::ListValue& args) {
  AllowJavascript();
  const std::string& callback_id = args[0].GetString();
  const GURL server = NormalizeServerURL(args[1].GetString());
  if (!server.is_valid()) {
    RejectJavascriptCallback(callback_id,
                             base::Value("Enter the server's https address, such "
                                         "as https://sync.example.com."));
    return;
  }
  NewLogin login;
  login.login = syncer::NormalizeSyncLogin(args[2].GetString());
  login.password = args[3].GetString();
  if (login.login.empty() || login.password.empty()) {
    RejectJavascriptCallback(
        callback_id,
        base::Value("Enter the email or login name and the password."));
    return;
  }
  CheckServer(
      callback_id, server,
      base::BindOnce(
          [](base::WeakPtr<AlerseSyncHandler> self,
             const std::string& callback_id, const GURL& server,
             NewLogin login) {
            if (!self) {
              return;
            }
            const std::string name = login.login;
            const std::string password = login.password;
            self->login_client().Unlock(
                server, name, password,
                base::BindOnce(
                    [](base::WeakPtr<AlerseSyncHandler> self,
                       const std::string& callback_id, const GURL& server,
                       NewLogin login,
                       std::optional<syncer::SyncChainSecret> secret,
                       std::optional<std::string> error) {
                      if (!self) {
                        return;
                      }
                      if (!secret) {
                        self->RejectJavascriptCallback(
                            callback_id,
                            base::Value(error.value_or("Sign-in failed.")));
                        return;
                      }
                      self->Save(callback_id, server, *secret,
                                 std::move(login), base::DictValue());
                    },
                    self, callback_id, server, std::move(login)));
          },
          weak_factory_.GetWeakPtr(), callback_id, server, std::move(login)));
}

void AlerseSyncHandler::CheckServer(const std::string& callback_id,
                                      const GURL& server,
                                      base::OnceClosure next) {
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = server.GetWithEmptyPath().Resolve("health");
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  health_check_ = network::SimpleURLLoader::Create(std::move(request),
                                                   kHealthCheckAnnotation);
  health_check_->SetTimeoutDuration(kHealthCheckTimeout);
  health_check_->DownloadToString(
      profile_->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess()
          .get(),
      base::BindOnce(&AlerseSyncHandler::OnServerChecked,
                     weak_factory_.GetWeakPtr(), callback_id, std::move(next)),
      /*max_body_size=*/1024);
}

void AlerseSyncHandler::OnServerChecked(const std::string& callback_id,
                                          base::OnceClosure next,
                                          std::optional<std::string> body) {
  health_check_.reset();
  if (!body) {
    RejectJavascriptCallback(
        callback_id,
        base::Value("The server does not answer. Check the address and that "
                    "the server is running."));
    return;
  }
  std::move(next).Run();
}

void AlerseSyncHandler::Save(const std::string& callback_id,
                               const GURL& server,
                               const syncer::SyncChainSecret& secret,
                               std::optional<NewLogin> login,
                               base::DictValue result) {
  g_browser_process->os_crypt_async()->GetInstance(base::BindOnce(
      &AlerseSyncHandler::OnEncryptorForSave, weak_factory_.GetWeakPtr(),
      callback_id, server, secret, std::move(login), std::move(result)));
}

void AlerseSyncHandler::OnEncryptorForSave(
    const std::string& callback_id,
    const GURL& server,
    const syncer::SyncChainSecret& secret,
    std::optional<NewLogin> login,
    base::DictValue result,
    scoped_refptr<os_crypt_async::Encryptor> encryptor) {
  syncer::AlerseSyncAccount::Save(profile_->GetPrefs(), *encryptor, server,
                                    secret);
  if (login) {
    syncer::AlerseSyncAccount::SetLogin(profile_->GetPrefs(), login->login);
  } else {
    syncer::AlerseSyncAccount::ClearLogin(profile_->GetPrefs());
  }
  // Written now rather than later, so a crash cannot lose the setup.
  profile_->GetPrefs()->CommitPendingWrite();
  restart_needed_ = true;
  ResolveJavascriptCallback(callback_id, result);
  FireWebUIListener("state-changed", BuildState());
}

AlerseSyncLoginClient& AlerseSyncHandler::login_client() {
  if (!login_client_) {
    login_client_ = std::make_unique<AlerseSyncLoginClient>(
        profile_->GetDefaultStoragePartition()
            ->GetURLLoaderFactoryForBrowserProcess());
  }
  return *login_client_;
}

void AlerseSyncHandler::WithSecret(
    const std::string& callback_id,
    base::OnceCallback<void(const syncer::SyncChainSecret&)> next) {
  g_browser_process->os_crypt_async()->GetInstance(base::BindOnce(
      &AlerseSyncHandler::OnEncryptorForSecret, weak_factory_.GetWeakPtr(),
      callback_id, std::move(next)));
}

void AlerseSyncHandler::OnEncryptorForSecret(
    const std::string& callback_id,
    base::OnceCallback<void(const syncer::SyncChainSecret&)> next,
    scoped_refptr<os_crypt_async::Encryptor> encryptor) {
  const std::optional<syncer::SyncChainSecret> secret =
      syncer::AlerseSyncAccount::LoadSecret(*profile_->GetPrefs(),
                                              *encryptor);
  if (!secret ||
      !syncer::AlerseSyncAccount::GetServerURL(*profile_->GetPrefs())
           .is_valid()) {
    RejectJavascriptCallback(callback_id,
                             base::Value("This device has no sync code."));
    return;
  }
  std::move(next).Run(*secret);
}

void AlerseSyncHandler::HandleGetLoginState(const base::ListValue& args) {
  AllowJavascript();
  const std::string callback_id = args[0].GetString();
  WithSecret(
      callback_id,
      base::BindOnce(
          [](base::WeakPtr<AlerseSyncHandler> self,
             const std::string& callback_id,
             const syncer::SyncChainSecret& secret) {
            if (!self) {
              return;
            }
            const GURL server = syncer::AlerseSyncAccount::GetServerURL(
                *self->profile_->GetPrefs());
            self->login_client().GetLogin(
                server, secret,
                base::BindOnce(&AlerseSyncHandler::OnLoginState, self,
                               callback_id));
          },
          weak_factory_.GetWeakPtr(), callback_id));
}

void AlerseSyncHandler::OnLoginState(const std::string& callback_id,
                                       std::optional<std::string> id,
                                       std::optional<std::string> error) {
  PrefService* prefs = profile_->GetPrefs();
  const std::string login = syncer::AlerseSyncAccount::GetLogin(*prefs);
  std::string on_server = "unknown";
  if (id && *id == "none") {
    on_server = "none";
  } else if (id) {
    on_server =
        !login.empty() && *id == syncer::SyncLoginId(login) ? "this" : "other";
  }
  base::DictValue state;
  state.Set("login", login);
  state.Set("onServer", on_server);
  state.Set("error", error.value_or(std::string()));
  ResolveJavascriptCallback(callback_id, state);
}

void AlerseSyncHandler::HandleSetLogin(const base::ListValue& args) {
  AllowJavascript();
  const std::string callback_id = args[0].GetString();
  std::optional<NewLogin> login = ReadNewLogin(callback_id, args[1]);
  if (!login) {
    return;
  }
  WithSecret(
      callback_id,
      base::BindOnce(
          [](base::WeakPtr<AlerseSyncHandler> self,
             const std::string& callback_id, NewLogin login,
             const syncer::SyncChainSecret& secret) {
            if (!self) {
              return;
            }
            const GURL server = syncer::AlerseSyncAccount::GetServerURL(
                *self->profile_->GetPrefs());
            const std::string name = login.login;
            const std::string password = login.password;
            self->login_client().SetLogin(
                server, secret, name, password,
                base::BindOnce(&AlerseSyncHandler::OnLoginChanged, self,
                               callback_id, std::move(login)));
          },
          weak_factory_.GetWeakPtr(), callback_id, std::move(*login)));
}

void AlerseSyncHandler::HandleRemoveLogin(const base::ListValue& args) {
  AllowJavascript();
  const std::string callback_id = args[0].GetString();
  WithSecret(
      callback_id,
      base::BindOnce(
          [](base::WeakPtr<AlerseSyncHandler> self,
             const std::string& callback_id,
             const syncer::SyncChainSecret& secret) {
            if (!self) {
              return;
            }
            const GURL server = syncer::AlerseSyncAccount::GetServerURL(
                *self->profile_->GetPrefs());
            self->login_client().RemoveLogin(
                server, secret,
                base::BindOnce(&AlerseSyncHandler::OnLoginChanged, self,
                               callback_id, std::nullopt));
          },
          weak_factory_.GetWeakPtr(), callback_id));
}

void AlerseSyncHandler::OnLoginChanged(const std::string& callback_id,
                                         std::optional<NewLogin> login,
                                         std::optional<std::string> error) {
  if (error) {
    RejectJavascriptCallback(callback_id, base::Value(*error));
    return;
  }
  if (login) {
    syncer::AlerseSyncAccount::SetLogin(profile_->GetPrefs(), login->login);
  } else {
    syncer::AlerseSyncAccount::ClearLogin(profile_->GetPrefs());
  }
  ResolveJavascriptCallback(callback_id, base::DictValue());
}

void AlerseSyncHandler::HandleCheckPassword(const base::ListValue& args) {
  AllowJavascript();
  std::string problem;
  switch (syncer::CheckSyncPassword(
      args[2].GetString(), syncer::NormalizeSyncLogin(args[1].GetString()))) {
    case syncer::SyncPasswordProblem::kNone:
      break;
    case syncer::SyncPasswordProblem::kTooShort:
      problem = "tooShort";
      break;
    case syncer::SyncPasswordProblem::kTooLong:
      problem = "tooLong";
      break;
    case syncer::SyncPasswordProblem::kCommon:
      problem = "common";
      break;
    case syncer::SyncPasswordProblem::kContainsLogin:
      problem = "containsLogin";
      break;
  }
  base::DictValue result;
  result.Set("problem", problem);
  ResolveJavascriptCallback(args[0], result);
}

void AlerseSyncHandler::HandleShowCode(const base::ListValue& args) {
  AllowJavascript();
  g_browser_process->os_crypt_async()->GetInstance(
      base::BindOnce(&AlerseSyncHandler::OnEncryptorForCode,
                     weak_factory_.GetWeakPtr(), args[0].GetString()));
}

void AlerseSyncHandler::OnEncryptorForCode(
    const std::string& callback_id,
    scoped_refptr<os_crypt_async::Encryptor> encryptor) {
  const std::optional<syncer::SyncChainSecret> secret =
      syncer::AlerseSyncAccount::LoadSecret(*profile_->GetPrefs(),
                                              *encryptor);
  if (!secret) {
    RejectJavascriptCallback(callback_id,
                             base::Value("This device has no sync code."));
    return;
  }
  base::DictValue result;
  result.Set("code", syncer::EncodeSyncCode(*secret));
  ResolveJavascriptCallback(callback_id, result);
}

void AlerseSyncHandler::HandleLeaveChain(const base::ListValue& args) {
  AllowJavascript();
  // In Alerse the sync service is always a SyncServiceImpl.
  if (syncer::SyncService* sync = GetSyncService()) {
    static_cast<syncer::SyncServiceImpl*>(sync)->LeaveAlerseSyncChain();
  } else {
    syncer::AlerseSyncAccount::Clear(profile_->GetPrefs());
  }
  profile_->GetPrefs()->CommitPendingWrite();
  restart_needed_ = true;
  ResolveJavascriptCallback(args[0], base::DictValue());
  FireWebUIListener("state-changed", BuildState());
}

void AlerseSyncHandler::HandleRestart(const base::ListValue& args) {
  chrome::AttemptRestart();
}

// Pairing ---------------------------------------------------------------------

void AlerseSyncHandler::ResetPairing() {
  pairing_.reset();
  pairing_code_.reset();
  pairing_secret_.reset();
  pairing_server_ = GURL();
  pairing_name_.clear();
}

void AlerseSyncHandler::HandleStartPairing(const base::ListValue& args) {
  AllowJavascript();
  const std::string callback_id = args[0].GetString();
  WithSecret(
      callback_id,
      base::BindOnce(
          [](base::WeakPtr<AlerseSyncHandler> self,
             const std::string& callback_id,
             const syncer::SyncChainSecret& secret) {
            if (!self) {
              return;
            }
            self->ResetPairing();
            self->pairing_server_ = syncer::AlerseSyncAccount::GetServerURL(
                *self->profile_->GetPrefs());
            self->pairing_secret_ = secret;
            self->pairing_code_ = syncer::GenerateSyncPairingCode();
            self->pairing_ = std::make_unique<syncer::SyncPairing>(
                syncer::SyncPairing::Role::kExisting, *self->pairing_code_);
            base::DictValue body;
            body.Set("slot", self->pairing_code_->SlotId());
            body.Set("msg", base::Base64Encode(self->pairing_->message()));
            self->login_client().Send(
                self->pairing_server_, "POST", "pair", secret, std::move(body),
                base::BindOnce(&AlerseSyncHandler::OnPairingOpened, self,
                               callback_id));
          },
          weak_factory_.GetWeakPtr(), callback_id));
}

void AlerseSyncHandler::OnPairingOpened(
    const std::string& callback_id,
    AlerseSyncLoginClient::Response response) {
  if (response.status != net::HTTP_NO_CONTENT || !pairing_code_) {
    ResetPairing();
    RejectJavascriptCallback(
        callback_id,
        base::Value(response.status == net::HTTP_CONFLICT
                        ? std::string("Could not open a pairing. Try again "
                                      "in a moment.")
                        : AlerseSyncLoginClient::ErrorMessage(response)));
    return;
  }
  const std::string code = pairing_code_->Display();
  // For phones: the future iOS app reads the server and the code from it.
  const std::string link =
      "alerse://pair?server=" +
      base::EscapeQueryParamValue(pairing_server_.spec(), /*use_plus=*/false) +
      "&code=" + code;
  base::DictValue result;
  result.Set("code", code);
  result.Set("link", link);
  auto qr = qr_code_generator::GenerateCode(base::as_byte_span(link));
  if (qr.has_value()) {
    std::string modules;
    modules.reserve(qr->data.size());
    for (uint8_t module : qr->data) {
      modules.push_back((module & 1) ? '1' : '0');
    }
    base::DictValue qr_dict;
    qr_dict.Set("size", qr->qr_size);
    qr_dict.Set("modules", modules);
    result.Set("qr", std::move(qr_dict));
  }
  ResolveJavascriptCallback(callback_id, result);
}

void AlerseSyncHandler::HandlePairingStatus(const base::ListValue& args) {
  AllowJavascript();
  const std::string callback_id = args[0].GetString();
  if (!pairing_code_ || !pairing_secret_) {
    base::DictValue state;
    state.Set("state", "expired");
    ResolveJavascriptCallback(callback_id, state);
    return;
  }
  login_client().Send(pairing_server_, "GET",
                      "pair/" + pairing_code_->SlotId(), std::nullopt,
                      std::nullopt,
                      base::BindOnce(&AlerseSyncHandler::OnPairingSlot,
                                     weak_factory_.GetWeakPtr(), callback_id));
}

void AlerseSyncHandler::OnPairingSlot(
    const std::string& callback_id,
    AlerseSyncLoginClient::Response response) {
  base::DictValue state;
  if (!pairing_ || !pairing_code_) {
    state.Set("state", "expired");
    ResolveJavascriptCallback(callback_id, state);
    return;
  }
  if (response.status == net::HTTP_NOT_FOUND) {
    ResetPairing();
    state.Set("state", "expired");
    ResolveJavascriptCallback(callback_id, state);
    return;
  }
  if (response.status != net::HTTP_OK || !response.json) {
    // Try again at the next poll.
    state.Set("state", "waiting");
    state.Set("error", AlerseSyncLoginClient::ErrorMessage(response));
    ResolveJavascriptCallback(callback_id, state);
    return;
  }
  const base::DictValue& slot = *response.json;
  state.Set("expires", slot.FindInt("expires").value_or(0));
  if (slot.FindString("box")) {
    state.Set("state", "sent");
    ResolveJavascriptCallback(callback_id, state);
    return;
  }
  if (!slot.FindBool("joined").value_or(false)) {
    state.Set("state", "waiting");
    ResolveJavascriptCallback(callback_id, state);
    return;
  }
  const std::string* bmsg64 = slot.FindString("bmsg");
  const std::string* name = slot.FindString("name");
  const std::string* confirm64 = slot.FindString("confirm");
  std::optional<std::vector<uint8_t>> bmsg =
      bmsg64 ? base::Base64Decode(*bmsg64) : std::nullopt;
  std::optional<std::vector<uint8_t>> confirm =
      confirm64 ? base::Base64Decode(*confirm64) : std::nullopt;
  bool ok = bmsg && name && confirm;
  if (ok && !pairing_->finished()) {
    ok = pairing_->Finish(*bmsg);
  }
  ok = ok && pairing_->CheckConfirm(*name, *confirm);
  if (!ok) {
    // The other device typed another code, or someone guessed at the slot:
    // close it, so nothing more can be tried on this code.
    login_client().Send(pairing_server_, "DELETE",
                        "pair/" + pairing_code_->SlotId(), pairing_secret_,
                        std::nullopt, base::DoNothing());
    ResetPairing();
    state.Set("state", "wrongCode");
    ResolveJavascriptCallback(callback_id, state);
    return;
  }
  pairing_name_ = *name;
  state.Set("state", "joined");
  state.Set("name", *name);
  state.Set("number", pairing_->VerificationNumber());
  ResolveJavascriptCallback(callback_id, state);
}

void AlerseSyncHandler::HandleApprovePairing(const base::ListValue& args) {
  AllowJavascript();
  const std::string callback_id = args[0].GetString();
  if (!pairing_ || !pairing_->finished() || pairing_name_.empty() ||
      !pairing_secret_) {
    RejectJavascriptCallback(callback_id,
                             base::Value("The pairing ended. Start again."));
    return;
  }
  base::DictValue body;
  body.Set("box", base::Base64Encode(pairing_->Seal(*pairing_secret_)));
  login_client().Send(
      pairing_server_, "POST", "pair/" + pairing_code_->SlotId() + "/approve",
      pairing_secret_, std::move(body),
      base::BindOnce(
          [](base::WeakPtr<AlerseSyncHandler> self,
             const std::string& callback_id,
             AlerseSyncLoginClient::Response response) {
            if (!self) {
              return;
            }
            self->ResetPairing();
            if (response.status != net::HTTP_NO_CONTENT) {
              self->RejectJavascriptCallback(
                  callback_id,
                  base::Value(response.status == net::HTTP_NOT_FOUND
                                  ? std::string("The code expired. Start "
                                                "again.")
                                  : AlerseSyncLoginClient::ErrorMessage(
                                        response)));
              return;
            }
            self->ResolveJavascriptCallback(callback_id, base::DictValue());
          },
          weak_factory_.GetWeakPtr(), callback_id));
}

void AlerseSyncHandler::HandleCancelPairing(const base::ListValue& args) {
  AllowJavascript();
  if (pairing_code_ && pairing_secret_) {
    login_client().Send(pairing_server_, "DELETE",
                        "pair/" + pairing_code_->SlotId(), pairing_secret_,
                        std::nullopt, base::DoNothing());
  }
  ResetPairing();
  ResolveJavascriptCallback(args[0], base::DictValue());
}

void AlerseSyncHandler::HandleJoinWithPairing(const base::ListValue& args) {
  AllowJavascript();
  const std::string callback_id = args[0].GetString();
  const GURL server = NormalizeServerURL(args[1].GetString());
  if (!server.is_valid()) {
    RejectJavascriptCallback(callback_id,
                             base::Value("Enter the server's https address, such "
                                         "as https://sync.example.com."));
    return;
  }
  const std::optional<syncer::SyncPairingCode> code =
      syncer::ParseSyncPairingCode(args[2].GetString());
  if (!code) {
    RejectJavascriptCallback(
        callback_id, base::Value("That is not a pairing code. It has 12 "
                                 "letters and digits, like ABCD-EFGH-JKMN."));
    return;
  }
  ResetPairing();
  pairing_server_ = server;
  pairing_code_ = *code;
  pairing_ = std::make_unique<syncer::SyncPairing>(
      syncer::SyncPairing::Role::kNew, *code);
  CheckServer(
      callback_id, server,
      base::BindOnce(
          [](base::WeakPtr<AlerseSyncHandler> self,
             const std::string& callback_id) {
            if (!self || !self->pairing_code_) {
              return;
            }
            self->login_client().Send(
                self->pairing_server_, "GET",
                "pair/" + self->pairing_code_->SlotId(), std::nullopt,
                std::nullopt,
                base::BindOnce(&AlerseSyncHandler::OnPairingSlotForJoin,
                               self, callback_id));
          },
          weak_factory_.GetWeakPtr(), callback_id));
}

void AlerseSyncHandler::OnPairingSlotForJoin(
    const std::string& callback_id,
    AlerseSyncLoginClient::Response response) {
  if (!pairing_) {
    return;
  }
  if (response.status == net::HTTP_NOT_FOUND) {
    ResetPairing();
    RejectJavascriptCallback(
        callback_id,
        base::Value("No pairing with this code. Check the code; it works "
                    "for 5 minutes after the other device shows it."));
    return;
  }
  const std::string* msg64 =
      response.json ? response.json->FindString("msg") : nullptr;
  std::optional<std::vector<uint8_t>> msg =
      msg64 ? base::Base64Decode(*msg64) : std::nullopt;
  if (response.status != net::HTTP_OK || !msg || !pairing_->Finish(*msg)) {
    ResetPairing();
    RejectJavascriptCallback(
        callback_id,
        base::Value(AlerseSyncLoginClient::ErrorMessage(response)));
    return;
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&syncer::GetPersonalizableDeviceNameBlocking),
      base::BindOnce(&AlerseSyncHandler::OnDeviceNameForJoin,
                     weak_factory_.GetWeakPtr(), callback_id));
}

void AlerseSyncHandler::OnDeviceNameForJoin(const std::string& callback_id,
                                              const std::string& device_name) {
  if (!pairing_ || !pairing_code_) {
    return;
  }
  // At most 64 characters, as the server takes them.
  std::u16string name16 = base::UTF8ToUTF16(
      device_name.empty() ? std::string("New device") : device_name);
  name16 = name16.substr(0, 64);
  const std::string name = base::UTF16ToUTF8(name16);
  base::DictValue body;
  body.Set("msg", base::Base64Encode(pairing_->message()));
  body.Set("name", name);
  body.Set("confirm", base::Base64Encode(pairing_->Confirm(name)));
  login_client().Send(
      pairing_server_, "POST", "pair/" + pairing_code_->SlotId() + "/join",
      std::nullopt, std::move(body),
      base::BindOnce(
          [](base::WeakPtr<AlerseSyncHandler> self,
             const std::string& callback_id,
             AlerseSyncLoginClient::Response response) {
            if (!self || !self->pairing_) {
              return;
            }
            if (response.status != net::HTTP_NO_CONTENT) {
              const std::string error =
                  response.status == net::HTTP_CONFLICT
                      ? std::string("This code was used already. Make a new "
                                    "one on the other device.")
                  : response.status == net::HTTP_NOT_FOUND
                      ? std::string("The code expired. Make a new one on "
                                    "the other device.")
                      : AlerseSyncLoginClient::ErrorMessage(response);
              self->ResetPairing();
              self->RejectJavascriptCallback(callback_id, base::Value(error));
              return;
            }
            base::DictValue result;
            result.Set("number", self->pairing_->VerificationNumber());
            self->ResolveJavascriptCallback(callback_id, result);
          },
          weak_factory_.GetWeakPtr(), callback_id));
}

void AlerseSyncHandler::HandlePairingJoinStatus(const base::ListValue& args) {
  AllowJavascript();
  const std::string callback_id = args[0].GetString();
  if (!pairing_ || !pairing_code_ || !pairing_->finished()) {
    base::DictValue state;
    state.Set("state", "cancelled");
    ResolveJavascriptCallback(callback_id, state);
    return;
  }
  login_client().Send(
      pairing_server_, "GET", "pair/" + pairing_code_->SlotId(), std::nullopt,
      std::nullopt,
      base::BindOnce(&AlerseSyncHandler::OnPairingJoinSlot,
                     weak_factory_.GetWeakPtr(), callback_id));
}

void AlerseSyncHandler::OnPairingJoinSlot(
    const std::string& callback_id,
    AlerseSyncLoginClient::Response response) {
  base::DictValue state;
  if (!pairing_) {
    state.Set("state", "cancelled");
    ResolveJavascriptCallback(callback_id, state);
    return;
  }
  if (response.status == net::HTTP_NOT_FOUND) {
    // Denied, cancelled, or expired before the other device allowed it.
    ResetPairing();
    state.Set("state", "cancelled");
    ResolveJavascriptCallback(callback_id, state);
    return;
  }
  const std::string* box64 =
      response.json ? response.json->FindString("box") : nullptr;
  if (response.status != net::HTTP_OK || !box64) {
    state.Set("state", "waiting");
    ResolveJavascriptCallback(callback_id, state);
    return;
  }
  const std::optional<std::vector<uint8_t>> box = base::Base64Decode(*box64);
  const std::optional<syncer::SyncChainSecret> secret =
      box ? pairing_->Open(*box) : std::nullopt;
  const GURL server = pairing_server_;
  ResetPairing();
  if (!secret) {
    state.Set("state", "failed");
    ResolveJavascriptCallback(callback_id, state);
    return;
  }
  state.Set("state", "done");
  Save(callback_id, server, *secret, std::nullopt, std::move(state));
}
