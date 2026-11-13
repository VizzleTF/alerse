// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/alerse_sync/alerse_sync_handler.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/prefs/pref_service.h"
#include "components/sync/service/alerse_sync_code.h"
#include "components/sync/service/alerse_sync_sk.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync_device_info/local_device_info_util.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/url_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/icu/source/common/unicode/normalizer2.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "url/url_constants.h"

namespace {

// The official server, the page's first choice; any other is typed.
constexpr char kAlerseServer[] = "https://sync.alerse.app";

constexpr base::TimeDelta kHealthCheckTimeout = base::Seconds(10);

constexpr net::NetworkTrafficAnnotationTag kHealthCheckAnnotation =
    net::DefineNetworkTrafficAnnotation("alerse_sync_server_check", R"(
      semantics {
        sender: "Alerse Sync"
        description:
          "Checks that the sync server the user picked answers before the "
          "browser creates or signs in to an account on it."
        trigger: "The user sets up sync."
        data: "None."
        destination: OTHER
        destination_other: "The sync server the user picked."
      }
      policy {
        cookies_allowed: NO
        setting: "Not started unless the user sets up sync."
        policy_exception_justification: "Not implemented."
      })");

constexpr char kBadServer[] =
    "Enter the server's https address, such as https://sync.example.com.";
constexpr char kNoSetup[] = "This device is not signed in to a sync account.";

// Reads a server address as people type it: its origin, https only, but for
// a server on this computer: the data is end-to-end encrypted, its metadata
// and the device's token are not.
GURL NormalizeServerURL(const std::string& typed) {
  GURL url(typed);
  if (!url.is_valid() || url.host().empty() ||
      !(url.SchemeIs(url::kHttpsScheme) ||
        (url.SchemeIs(url::kHttpScheme) && net::IsLocalhost(url)))) {
    return GURL();
  }
  return url.GetWithEmptyPath();
}

// The password as the account's keys are derived from it: Unicode NFC, so
// that the same password typed on another system opens the account.
std::string ToNfc(const std::string& utf8) {
  UErrorCode status = U_ZERO_ERROR;
  const icu::Normalizer2* nfc = icu::Normalizer2::getNFCInstance(status);
  if (U_FAILURE(status)) {
    return utf8;
  }
  const icu::UnicodeString normalized =
      nfc->normalize(icu::UnicodeString::fromUTF8(utf8), status);
  if (U_FAILURE(status)) {
    return utf8;
  }
  std::string out;
  normalized.toUTF8String(out);
  return out;
}

// The message for a password problem, or empty.
std::string PasswordProblem(const std::string& password) {
  switch (syncer::CheckSyncPassword(password)) {
    case syncer::SyncPasswordProblem::kNone:
      return std::string();
    case syncer::SyncPasswordProblem::kTooShort:
      return "Use at least 15 characters. A few unrelated words make a good "
             "password.";
    case syncer::SyncPasswordProblem::kTooLong:
      return "Use at most 256 characters.";
    case syncer::SyncPasswordProblem::kCommon:
      return "This password is too easy to guess. Pick another one.";
    case syncer::SyncPasswordProblem::kContainsName:
      return "Do not use the browser's name in the password.";
  }
}

// The JavaScript time of `time`, or 0 for none.
double ToJsTime(base::Time time) {
  return time.is_null() ? 0 : time.InMillisecondsFSinceUnixEpoch();
}

}  // namespace

AlerseSyncHandler::AlerseSyncHandler(Profile* profile) : profile_(profile) {}

AlerseSyncHandler::~AlerseSyncHandler() = default;

void AlerseSyncHandler::RegisterMessages() {
  const auto add = [this](const char* name,
                          void (AlerseSyncHandler::*method)(
                              const base::ListValue&)) {
    web_ui()->RegisterMessageCallback(
        name, base::BindRepeating(method, base::Unretained(this)));
  };
  add("getState", &AlerseSyncHandler::HandleGetState);
  add("checkPassword", &AlerseSyncHandler::HandleCheckPassword);
  add("createAccount", &AlerseSyncHandler::HandleCreateAccount);
  add("signIn", &AlerseSyncHandler::HandleSignIn);
  add("showWords", &AlerseSyncHandler::HandleShowWords);
  add("getDevices", &AlerseSyncHandler::HandleGetDevices);
  add("removeDevice", &AlerseSyncHandler::HandleRemoveDevice);
  add("leave", &AlerseSyncHandler::HandleLeave);
  add("deleteAccount", &AlerseSyncHandler::HandleDeleteAccount);
  add("dismissLegacy", &AlerseSyncHandler::HandleDismissLegacy);
  add("restart", &AlerseSyncHandler::HandleRestart);
}

void AlerseSyncHandler::OnJavascriptAllowed() {
  if (syncer::SyncService* sync = GetSyncService()) {
    sync_observation_.Observe(sync);
  }
}

void AlerseSyncHandler::OnJavascriptDisallowed() {
  sync_observation_.Reset();
  health_check_.reset();
  if (api_) {
    api_->Cancel();
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

AlerseSyncApiClient& AlerseSyncHandler::api() {
  if (!api_) {
    api_ = std::make_unique<AlerseSyncApiClient>(
        profile_->GetDefaultStoragePartition()
            ->GetURLLoaderFactoryForBrowserProcess());
  }
  return *api_;
}

base::DictValue AlerseSyncHandler::BuildState() const {
  const PrefService& prefs = *profile_->GetPrefs();
  const GURL server = syncer::AlerseSyncAccount::GetServerURL(prefs);
  const syncer::SyncService* sync = GetSyncService();

  base::DictValue state;
  state.Set("server", server.is_valid() ? server.spec() : std::string());
  state.Set("defaultServer", kAlerseServer);
  state.Set("legacyReset", syncer::AlerseSyncAccount::WasLegacyReset(prefs));
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
      status = "wrongKey";
    } else if (sync->GetAuthError().state() != GoogleServiceAuthError::NONE) {
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

void AlerseSyncHandler::HandleCheckPassword(const base::ListValue& args) {
  AllowJavascript();
  base::DictValue result;
  result.Set("problem", PasswordProblem(ToNfc(args[1].GetString())));
  ResolveJavascriptCallback(args[0], result);
}

void AlerseSyncHandler::HandleCreateAccount(const base::ListValue& args) {
  AllowJavascript();
  const std::string callback_id = args[0].GetString();
  const GURL server = NormalizeServerURL(args[1].GetString());
  const std::string password = ToNfc(args[2].GetString());
  if (!server.is_valid()) {
    RejectJavascriptCallback(callback_id, base::Value(kBadServer));
    return;
  }
  if (password != ToNfc(args[3].GetString())) {
    RejectJavascriptCallback(callback_id,
                             base::Value("The passwords do not match."));
    return;
  }
  if (const std::string problem = PasswordProblem(password); !problem.empty()) {
    RejectJavascriptCallback(callback_id, base::Value(problem));
    return;
  }
  CheckServer(
      callback_id, server,
      base::BindOnce(
          [](base::WeakPtr<AlerseSyncHandler> self, std::string callback_id,
             GURL server, std::string password) {
            if (!self) {
              return;
            }
            self->WithDeviceName(base::BindOnce(
                [](base::WeakPtr<AlerseSyncHandler> self,
                   std::string callback_id, GURL server, std::string password,
                   const std::string& name) {
                  if (!self) {
                    return;
                  }
                  self->api().CreateAccount(
                      server, password, name,
                      base::BindOnce(&AlerseSyncHandler::OnSignedIn, self,
                                     callback_id, server, base::DictValue()));
                },
                self, callback_id, server, password));
          },
          weak_factory_.GetWeakPtr(), callback_id, server, password));
}

void AlerseSyncHandler::HandleSignIn(const base::ListValue& args) {
  AllowJavascript();
  const std::string callback_id = args[0].GetString();
  const GURL server = NormalizeServerURL(args[1].GetString());
  const std::optional<syncer::SyncChainSecret> words =
      syncer::DecodeSyncCode(args[2].GetString());
  const std::string password = ToNfc(args[3].GetString());
  if (!server.is_valid()) {
    RejectJavascriptCallback(callback_id, base::Value(kBadServer));
    return;
  }
  if (!words) {
    RejectJavascriptCallback(
        callback_id,
        base::Value("Those are not the 24 words. Check that all of them are "
                    "there, in order."));
    return;
  }
  if (password.empty()) {
    RejectJavascriptCallback(callback_id, base::Value("Enter the password."));
    return;
  }
  CheckServer(
      callback_id, server,
      base::BindOnce(
          [](base::WeakPtr<AlerseSyncHandler> self, std::string callback_id,
             GURL server, syncer::SyncChainSecret words, std::string password) {
            if (!self) {
              return;
            }
            self->WithDeviceName(base::BindOnce(
                [](base::WeakPtr<AlerseSyncHandler> self,
                   std::string callback_id, GURL server,
                   syncer::SyncChainSecret words, std::string password,
                   const std::string& name) {
                  if (!self) {
                    return;
                  }
                  self->api().SignIn(
                      server, words, password, name,
                      base::BindOnce(&AlerseSyncHandler::OnSignedIn, self,
                                     callback_id, server,
                                     base::DictValue().Set("joining", true)));
                },
                self, callback_id, server, words, password));
          },
          weak_factory_.GetWeakPtr(), callback_id, server, *words, password));
}

void AlerseSyncHandler::OnSignedIn(const std::string& callback_id,
                                   const GURL& server,
                                   base::DictValue result,
                                   std::optional<Setup> setup,
                                   std::optional<std::string> error) {
  if (!setup) {
    RejectJavascriptCallback(callback_id,
                             base::Value(error.value_or("Sign-in failed.")));
    return;
  }
  result.Set("words", syncer::EncodeSyncCode(setup->words));
  g_browser_process->os_crypt_async()->GetInstance(base::BindOnce(
      &AlerseSyncHandler::OnEncryptorForSave, weak_factory_.GetWeakPtr(),
      callback_id, server, std::move(*setup), std::move(result)));
}

void AlerseSyncHandler::OnEncryptorForSave(
    const std::string& callback_id,
    const GURL& server,
    Setup setup,
    base::DictValue result,
    scoped_refptr<os_crypt_async::Encryptor> encryptor) {
  if (!syncer::AlerseSyncAccount::Save(profile_->GetPrefs(), *encryptor,
                                       server, setup)) {
    RejectJavascriptCallback(
        callback_id,
        base::Value("Alerse could not keep the account's keys: the system's "
                    "keychain is not available. Unlock it and try again."));
    return;
  }
  // A device that signs in may have Spaces of its own; they wait for the
  // account's list (AlerseSyncAccount::IsJoining).
  syncer::AlerseSyncAccount::SetJoining(
      profile_->GetPrefs(), result.FindBool("joining").value_or(false));
  result.Remove("joining");
  // Written now rather than later, so a crash cannot lose the setup.
  profile_->GetPrefs()->CommitPendingWrite();
  g_browser_process->local_state()->CommitPendingWrite();
  restart_needed_ = true;
  ResolveJavascriptCallback(callback_id, result);
  FireWebUIListener("state-changed", BuildState());
}

void AlerseSyncHandler::CheckServer(const std::string& callback_id,
                                    const GURL& server,
                                    base::OnceClosure next) {
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = server.Resolve("/health");
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

void AlerseSyncHandler::WithDeviceName(
    base::OnceCallback<void(const std::string&)> next) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&syncer::GetPersonalizableDeviceNameBlocking),
      std::move(next));
}

void AlerseSyncHandler::WithSetup(const std::string& callback_id,
                                  base::OnceCallback<void(const Setup&)> next) {
  g_browser_process->os_crypt_async()->GetInstance(base::BindOnce(
      &AlerseSyncHandler::OnEncryptorForSetup, weak_factory_.GetWeakPtr(),
      callback_id, std::move(next)));
}

void AlerseSyncHandler::OnEncryptorForSetup(
    const std::string& callback_id,
    base::OnceCallback<void(const Setup&)> next,
    scoped_refptr<os_crypt_async::Encryptor> encryptor) {
  const std::optional<Setup> setup =
      syncer::AlerseSyncAccount::LoadSetup(*profile_->GetPrefs(), *encryptor);
  if (!setup ||
      !syncer::AlerseSyncAccount::GetServerURL(*profile_->GetPrefs())
           .is_valid()) {
    RejectJavascriptCallback(callback_id, base::Value(kNoSetup));
    return;
  }
  std::move(next).Run(*setup);
}

void AlerseSyncHandler::HandleShowWords(const base::ListValue& args) {
  AllowJavascript();
  const std::string callback_id = args[0].GetString();
  WithSetup(callback_id,
            base::BindOnce(
                [](base::WeakPtr<AlerseSyncHandler> self,
                   std::string callback_id, const Setup& setup) {
                  if (!self) {
                    return;
                  }
                  base::DictValue result;
                  result.Set("words", syncer::EncodeSyncCode(setup.words));
                  self->ResolveJavascriptCallback(base::Value(callback_id),
                                                  result);
                },
                weak_factory_.GetWeakPtr(), callback_id));
}

void AlerseSyncHandler::HandleGetDevices(const base::ListValue& args) {
  AllowJavascript();
  const std::string callback_id = args[0].GetString();
  WithSetup(
      callback_id,
      base::BindOnce(
          [](base::WeakPtr<AlerseSyncHandler> self, std::string callback_id,
             const Setup& setup) {
            if (!self) {
              return;
            }
            self->api().ListDevices(
                syncer::AlerseSyncAccount::GetServerURL(
                    *self->profile_->GetPrefs()),
                setup,
                base::BindOnce(
                    [](base::WeakPtr<AlerseSyncHandler> self,
                       std::string callback_id,
                       std::optional<std::vector<AlerseSyncApiClient::Device>>
                           devices,
                       std::optional<std::string> error) {
                      if (!self) {
                        return;
                      }
                      if (!devices) {
                        self->RejectJavascriptCallback(
                            base::Value(callback_id),
                            base::Value(error.value_or("")));
                        return;
                      }
                      base::ListValue list;
                      for (const AlerseSyncApiClient::Device& device :
                           *devices) {
                        base::DictValue item;
                        item.Set("id", device.id);
                        item.Set("name", device.name);
                        item.Set("created", device.created);
                        item.Set("lastSeen", device.last_seen);
                        item.Set("current", device.current);
                        list.Append(std::move(item));
                      }
                      base::DictValue result;
                      result.Set("devices", std::move(list));
                      self->ResolveJavascriptCallback(base::Value(callback_id),
                                                      result);
                    },
                    self, callback_id));
          },
          weak_factory_.GetWeakPtr(), callback_id));
}

void AlerseSyncHandler::HandleRemoveDevice(const base::ListValue& args) {
  AllowJavascript();
  const std::string callback_id = args[0].GetString();
  const std::string id = args[1].GetString();
  const std::string password = ToNfc(args[2].GetString());
  if (id.size() != 64 ||
      id.find_first_not_of("0123456789abcdef") != std::string::npos) {
    RejectJavascriptCallback(callback_id, base::Value("Unknown device."));
    return;
  }
  WithSetup(
      callback_id,
      base::BindOnce(
          [](base::WeakPtr<AlerseSyncHandler> self, std::string callback_id,
             std::string id, std::string password, const Setup& setup) {
            if (!self) {
              return;
            }
            self->api().RemoveDevice(
                syncer::AlerseSyncAccount::GetServerURL(
                    *self->profile_->GetPrefs()),
                setup, id, password,
                base::BindOnce(
                    [](base::WeakPtr<AlerseSyncHandler> self,
                       std::string callback_id,
                       std::optional<std::string> error) {
                      if (!self) {
                        return;
                      }
                      if (error) {
                        self->RejectJavascriptCallback(base::Value(callback_id),
                                                       base::Value(*error));
                        return;
                      }
                      self->ResolveJavascriptCallback(base::Value(callback_id),
                                                      base::DictValue());
                    },
                    self, callback_id));
          },
          weak_factory_.GetWeakPtr(), callback_id, id, password));
}

void AlerseSyncHandler::ForgetSetup() {
  // In Alerse the sync service is always a SyncServiceImpl.
  if (syncer::SyncService* sync = GetSyncService()) {
    static_cast<syncer::SyncServiceImpl*>(sync)->LeaveAlerseSyncChain();
  } else {
    syncer::AlerseSyncAccount::Clear(profile_->GetPrefs());
  }
  profile_->GetPrefs()->CommitPendingWrite();
  g_browser_process->local_state()->CommitPendingWrite();
  restart_needed_ = true;
}

void AlerseSyncHandler::HandleLeave(const base::ListValue& args) {
  AllowJavascript();
  ForgetSetup();
  ResolveJavascriptCallback(args[0], base::DictValue());
  FireWebUIListener("state-changed", BuildState());
}

void AlerseSyncHandler::HandleDeleteAccount(const base::ListValue& args) {
  AllowJavascript();
  const std::string callback_id = args[0].GetString();
  const std::string password = ToNfc(args[1].GetString());
  WithSetup(
      callback_id,
      base::BindOnce(
          [](base::WeakPtr<AlerseSyncHandler> self, std::string callback_id,
             std::string password, const Setup& setup) {
            if (!self) {
              return;
            }
            self->api().DeleteAccount(
                syncer::AlerseSyncAccount::GetServerURL(
                    *self->profile_->GetPrefs()),
                setup, password,
                base::BindOnce(
                    [](base::WeakPtr<AlerseSyncHandler> self,
                       std::string callback_id,
                       std::optional<std::string> error) {
                      if (!self) {
                        return;
                      }
                      if (error) {
                        self->RejectJavascriptCallback(base::Value(callback_id),
                                                       base::Value(*error));
                        return;
                      }
                      self->ForgetSetup();
                      self->ResolveJavascriptCallback(base::Value(callback_id),
                                                      base::DictValue());
                      self->FireWebUIListener("state-changed",
                                              self->BuildState());
                    },
                    self, callback_id));
          },
          weak_factory_.GetWeakPtr(), callback_id, password));
}

void AlerseSyncHandler::HandleDismissLegacy(const base::ListValue& args) {
  AllowJavascript();
  syncer::AlerseSyncAccount::ClearLegacyReset(profile_->GetPrefs());
  ResolveJavascriptCallback(args[0], base::DictValue());
  FireWebUIListener("state-changed", BuildState());
}

void AlerseSyncHandler::HandleRestart(const base::ListValue& args) {
  chrome::AttemptRestart();
}
