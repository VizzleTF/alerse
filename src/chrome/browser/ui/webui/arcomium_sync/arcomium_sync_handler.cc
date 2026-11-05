// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/arcomium_sync/arcomium_sync_handler.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/prefs/pref_service.h"
#include "components/sync/service/arcomium_sync_account.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync/service/sync_user_settings.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/url_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/url_constants.h"

namespace {

constexpr base::TimeDelta kHealthCheckTimeout = base::Seconds(10);

constexpr net::NetworkTrafficAnnotationTag kHealthCheckAnnotation =
    net::DefineNetworkTrafficAnnotation("arcomium_sync_server_check", R"(
      semantics {
        sender: "Arcomium Sync"
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

ArcomiumSyncHandler::ArcomiumSyncHandler(Profile* profile)
    : profile_(profile) {}

ArcomiumSyncHandler::~ArcomiumSyncHandler() = default;

void ArcomiumSyncHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getState", base::BindRepeating(&ArcomiumSyncHandler::HandleGetState,
                                      base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "createChain",
      base::BindRepeating(&ArcomiumSyncHandler::HandleCreateChain,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "joinChain", base::BindRepeating(&ArcomiumSyncHandler::HandleJoinChain,
                                       base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "showCode", base::BindRepeating(&ArcomiumSyncHandler::HandleShowCode,
                                      base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "leaveChain",
      base::BindRepeating(&ArcomiumSyncHandler::HandleLeaveChain,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "restart", base::BindRepeating(&ArcomiumSyncHandler::HandleRestart,
                                     base::Unretained(this)));
}

void ArcomiumSyncHandler::OnJavascriptAllowed() {
  if (syncer::SyncService* sync = GetSyncService()) {
    sync_observation_.Observe(sync);
  }
}

void ArcomiumSyncHandler::OnJavascriptDisallowed() {
  sync_observation_.Reset();
  health_check_.reset();
  weak_factory_.InvalidateWeakPtrs();
}

void ArcomiumSyncHandler::OnStateChanged(syncer::SyncService* sync) {
  FireWebUIListener("state-changed", BuildState());
}

void ArcomiumSyncHandler::OnSyncShutdown(syncer::SyncService* sync) {
  sync_observation_.Reset();
}

syncer::SyncService* ArcomiumSyncHandler::GetSyncService() const {
  return SyncServiceFactory::GetForProfile(profile_);
}

base::DictValue ArcomiumSyncHandler::BuildState() const {
  const GURL server =
      syncer::ArcomiumSyncAccount::GetServerURL(*profile_->GetPrefs());
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

void ArcomiumSyncHandler::HandleGetState(const base::ListValue& args) {
  AllowJavascript();
  ResolveJavascriptCallback(args[0], BuildState());
}

void ArcomiumSyncHandler::HandleCreateChain(const base::ListValue& args) {
  AllowJavascript();
  const std::string& callback_id = args[0].GetString();
  const GURL server = NormalizeServerURL(args[1].GetString());
  if (!server.is_valid()) {
    RejectJavascriptCallback(callback_id,
                             base::Value("Enter the server's https address, such "
                                         "as https://sync.example.com."));
    return;
  }
  const syncer::SyncChainSecret secret = syncer::GenerateSyncChainSecret();
  base::DictValue result;
  result.Set("code", syncer::EncodeSyncCode(secret));
  CheckServerAndSave(callback_id, server, secret, std::move(result));
}

void ArcomiumSyncHandler::HandleJoinChain(const base::ListValue& args) {
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
  CheckServerAndSave(callback_id, server, *secret, base::DictValue());
}

void ArcomiumSyncHandler::CheckServerAndSave(
    const std::string& callback_id,
    const GURL& server,
    const syncer::SyncChainSecret& secret,
    base::DictValue result) {
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
      base::BindOnce(&ArcomiumSyncHandler::OnServerChecked,
                     weak_factory_.GetWeakPtr(), callback_id, server, secret,
                     std::move(result)),
      /*max_body_size=*/1024);
}

void ArcomiumSyncHandler::OnServerChecked(const std::string& callback_id,
                                          const GURL& server,
                                          const syncer::SyncChainSecret& secret,
                                          base::DictValue result,
                                          std::optional<std::string> body) {
  health_check_.reset();
  if (!body) {
    RejectJavascriptCallback(
        callback_id,
        base::Value("The server does not answer. Check the address and that "
                    "the server is running."));
    return;
  }
  g_browser_process->os_crypt_async()->GetInstance(base::BindOnce(
      &ArcomiumSyncHandler::OnEncryptorForSave, weak_factory_.GetWeakPtr(),
      callback_id, server, secret, std::move(result)));
}

void ArcomiumSyncHandler::OnEncryptorForSave(
    const std::string& callback_id,
    const GURL& server,
    const syncer::SyncChainSecret& secret,
    base::DictValue result,
    scoped_refptr<os_crypt_async::Encryptor> encryptor) {
  syncer::ArcomiumSyncAccount::Save(profile_->GetPrefs(), *encryptor, server,
                                    secret);
  // Written now rather than later, so a crash cannot lose the setup.
  profile_->GetPrefs()->CommitPendingWrite();
  restart_needed_ = true;
  ResolveJavascriptCallback(callback_id, result);
  FireWebUIListener("state-changed", BuildState());
}

void ArcomiumSyncHandler::HandleShowCode(const base::ListValue& args) {
  AllowJavascript();
  g_browser_process->os_crypt_async()->GetInstance(
      base::BindOnce(&ArcomiumSyncHandler::OnEncryptorForCode,
                     weak_factory_.GetWeakPtr(), args[0].GetString()));
}

void ArcomiumSyncHandler::OnEncryptorForCode(
    const std::string& callback_id,
    scoped_refptr<os_crypt_async::Encryptor> encryptor) {
  const std::optional<syncer::SyncChainSecret> secret =
      syncer::ArcomiumSyncAccount::LoadSecret(*profile_->GetPrefs(),
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

void ArcomiumSyncHandler::HandleLeaveChain(const base::ListValue& args) {
  AllowJavascript();
  // In Arcomium the sync service is always a SyncServiceImpl.
  if (syncer::SyncService* sync = GetSyncService()) {
    static_cast<syncer::SyncServiceImpl*>(sync)->LeaveArcomiumSyncChain();
  } else {
    syncer::ArcomiumSyncAccount::Clear(profile_->GetPrefs());
  }
  profile_->GetPrefs()->CommitPendingWrite();
  restart_needed_ = true;
  ResolveJavascriptCallback(args[0], base::DictValue());
  FireWebUIListener("state-changed", BuildState());
}

void ArcomiumSyncHandler::HandleRestart(const base::ListValue& args) {
  chrome::AttemptRestart();
}
