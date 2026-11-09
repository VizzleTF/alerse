// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/arcomium_update/arcomium_update_checker.h"

#include <algorithm>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/arcomium_update/arcomium_version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/headless/headless_mode_util.h"
#include "chrome/common/chrome_switches.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/arcomium_update/arcomium_legacy_install_win.h"
#endif

namespace arcomium {

namespace {

// The repository this platform's builds are released from. The repositories
// were Arcomium's (VizzleTF/arcomium-win, ...); GitHub redirects the old names
// (the API answers 301 to /repositories/ID/..., which SimpleURLLoader
// follows), so Arcomium's builds, which ask for them, keep finding releases.
#if BUILDFLAG(IS_WIN)
constexpr char kRepository[] = "VizzleTF/alerse-win";
#elif BUILDFLAG(IS_MAC)
constexpr char kRepository[] = "VizzleTF/alerse-mac";
#else
constexpr char kRepository[] = "VizzleTF/alerse-linux";
#endif

// The first check waits for startup to settle; later ones come once a day. A
// check that gets no answer (offline, rate-limited, a server error) is
// retried sooner.
constexpr base::TimeDelta kStartupDelay = base::Minutes(5);
constexpr base::TimeDelta kCheckInterval = base::Days(1);
constexpr base::TimeDelta kRetryDelay = base::Hours(1);

// A release's JSON, with its notes and assets, is a few kilobytes.
constexpr size_t kMaxResponseSize = 1024 * 1024;

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("arcomium_update_check", R"(
        semantics {
          sender: "Alerse update check"
          description:
            "Asks GitHub's API for the latest Alerse release of this "
            "platform, to tell the user in the browser's menu when a newer "
            "one is out."
          trigger:
            "A few minutes after the browser starts, then once a day."
          data:
            "None: a plain GET of a public URL, without cookies."
          destination: OTHER
          destination_other: "GitHub's API (api.github.com)."
        }
        policy {
          cookies_allowed: NO
          setting:
            "Off when the Local State pref arcomium.update_check.enabled is "
            "false, and in headless mode or with "
            "--disable-background-networking."
          policy_exception_justification:
            "Alerse has no enterprise policy for it."
        })");

// A version of exactly MAJOR.MINOR.PATCH.
std::optional<base::Version> ParseVersion(std::string_view text) {
  if (text.empty()) {
    return std::nullopt;
  }
  for (char c : text) {
    if (c != '.' && (c < '0' || c > '9')) {
      return std::nullopt;
    }
  }
  base::Version version(text);
  if (!version.IsValid() || version.components().size() != 3) {
    return std::nullopt;
  }
  return version;
}

}  // namespace

void RegisterUpdateCheckPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kUpdateCheckEnabled, true);
  registry->RegisterTimePref(kUpdateCheckLastTime, base::Time());
  registry->RegisterStringPref(kUpdateCheckLatest, std::string());
}

GURL GetLatestReleaseUrl() {
  return GURL(
      base::StrCat({"https://github.com/", kRepository, "/releases/latest"}));
}

GURL GetReleaseUrl(std::string_view version) {
  return GURL(base::StrCat(
      {"https://github.com/", kRepository, "/releases/tag/v", version}));
}

std::optional<base::Version> ParseReleaseTag(std::string_view tag) {
  if (tag.starts_with('v')) {
    tag.remove_prefix(1);
  }
  return ParseVersion(tag);
}

bool IsNewerRelease(std::string_view latest, std::string_view running) {
  std::optional<base::Version> latest_version = ParseVersion(latest);
  std::optional<base::Version> running_version = ParseVersion(running);
  return latest_version && running_version &&
         *latest_version > *running_version;
}

bool IsAnswer(int http_status) {
  // GitHub's rate limit answers 403 or 429.
  return http_status != 0 && http_status != net::HTTP_FORBIDDEN &&
         http_status != net::HTTP_TOO_MANY_REQUESTS &&
         http_status < net::HTTP_INTERNAL_SERVER_ERROR;
}

std::string GetAvailableUpdate(const PrefService* local_state) {
  if (!local_state || !local_state->GetBoolean(kUpdateCheckEnabled)) {
    return std::string();
  }
  std::string latest = local_state->GetString(kUpdateCheckLatest);
  return IsNewerRelease(latest, ARCOMIUM_VERSION) ? latest : std::string();
}

UpdateChecker::UpdateChecker() = default;

UpdateChecker::~UpdateChecker() = default;

void UpdateChecker::PostBrowserStart() {
#if BUILDFLAG(IS_WIN)
  // Arcomium, which Alerse was, goes once Alerse took over its profile.
  if (!headless::IsHeadlessMode()) {
    RemoveLegacyInstallLater();
  }
#endif
  // A build that does not know its version has nothing to compare, and
  // automation asks for no background requests.
  if (!ParseVersion(ARCOMIUM_VERSION) || headless::IsHeadlessMode() ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableBackgroundNetworking)) {
    return;
  }
  local_state_ = g_browser_process->local_state();
  if (!local_state_ || !local_state_->GetBoolean(kUpdateCheckEnabled)) {
    return;
  }
  // Wait out the rest of the day since the last answer, if there is any.
  base::TimeDelta delay = kStartupDelay;
  base::TimeDelta until_due = local_state_->GetTime(kUpdateCheckLastTime) +
                              kCheckInterval - base::Time::Now();
  if (until_due > delay) {
    delay = std::min(until_due, kCheckInterval);
  }
  ScheduleCheck(delay);
}

void UpdateChecker::PostMainMessageLoopRun() {
  timer_.Stop();
  loader_.reset();
  local_state_ = nullptr;
}

void UpdateChecker::ScheduleCheck(base::TimeDelta delay) {
  timer_.Start(FROM_HERE, delay,
               base::BindOnce(&UpdateChecker::Check, base::Unretained(this)));
}

void UpdateChecker::Check() {
  if (!local_state_ || !local_state_->GetBoolean(kUpdateCheckEnabled)) {
    return;
  }
  scoped_refptr<network::SharedURLLoaderFactory> factory =
      g_browser_process->shared_url_loader_factory();
  if (!factory) {
    return;
  }
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL(base::StrCat(
      {"https://api.github.com/repos/", kRepository, "/releases/latest"}));
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  request->headers.SetHeader("Accept", "application/vnd.github+json");
  loader_ =
      network::SimpleURLLoader::Create(std::move(request), kTrafficAnnotation);
  // Only a successful answer has a body; an HTTP error comes back as nullopt
  // with its status in ResponseInfo().
  loader_->DownloadToString(
      factory.get(),
      base::BindOnce(&UpdateChecker::OnResponse, base::Unretained(this)),
      kMaxResponseSize);
}

void UpdateChecker::OnResponse(std::optional<std::string> body) {
  const network::mojom::URLResponseHead* head = loader_->ResponseInfo();
  const int status =
      head && head->headers ? head->headers->response_code() : 0;
  loader_.reset();
  if (!IsAnswer(status)) {
    ScheduleCheck(kRetryDelay);
    return;
  }
  // An answer that names no release this build knows (none yet, a tag that
  // is not vMAJOR.MINOR.PATCH, a broken body) keeps the last one found, and
  // the next check is a day away all the same.
  std::optional<base::Version> latest;
  if (body) {
    std::optional<base::DictValue> release =
        base::JSONReader::ReadDict(*body, base::JSON_PARSE_RFC);
    if (const std::string* tag =
            release ? release->FindString("tag_name") : nullptr) {
      latest = ParseReleaseTag(*tag);
    }
  }
  if (latest) {
    local_state_->SetString(kUpdateCheckLatest, latest->GetString());
  }
  local_state_->SetTime(kUpdateCheckLastTime, base::Time::Now());
  ScheduleCheck(kCheckInterval);
}

}  // namespace arcomium
