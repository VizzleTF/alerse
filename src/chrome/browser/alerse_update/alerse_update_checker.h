// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ALERSE_UPDATE_ALERSE_UPDATE_CHECKER_H_
#define CHROME_BROWSER_ALERSE_UPDATE_ALERSE_UPDATE_CHECKER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/version.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "url/gurl.h"

class PrefRegistrySimple;
class PrefService;

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace alerse {

// Local State prefs of the update check.
// Whether Alerse looks for a newer release once a day. On by default.
inline constexpr char kUpdateCheckEnabled[] = "arcomium.update_check.enabled";
// When the last check got an answer.
inline constexpr char kUpdateCheckLastTime[] =
    "arcomium.update_check.last_time";
// The latest release that answer named, as MAJOR.MINOR.PATCH.
inline constexpr char kUpdateCheckLatest[] = "arcomium.update_check.latest";

void RegisterUpdateCheckPrefs(PrefRegistrySimple* registry);

// The page of this platform's latest release.
GURL GetLatestReleaseUrl();

// The page of this platform's release `version` (MAJOR.MINOR.PATCH).
GURL GetReleaseUrl(std::string_view version);

// The version of a release tag, "vMAJOR.MINOR.PATCH" (the "v" may be left
// out), or nullopt for any other tag.
std::optional<base::Version> ParseReleaseTag(std::string_view tag);

// The newest release in GitHub's answer `body`: one release (the answer to
// /releases/latest) or a list of them (/releases), of which drafts and tags
// that are not releases are skipped. nullopt when it names none.
std::optional<base::Version> NewestRelease(std::string_view body);

// Whether `latest`, a release's MAJOR.MINOR.PATCH, is newer than `running`.
// False when either is not such a version.
bool IsNewerRelease(std::string_view latest, std::string_view running);

// Whether a check that got HTTP status `http_status` (0: no response) got an
// answer, which waits a day for the next check, rather than none (offline,
// rate-limited, a server error), which is retried sooner.
bool IsAnswer(int http_status);

// The newer release a check found, as MAJOR.MINOR.PATCH, or "" when there is
// none, this build does not know its own version or checks are off.
std::string GetAvailableUpdate(const PrefService* local_state);

// Asks GitHub for this platform's latest release a few minutes after startup
// and then once a day, and keeps its version in kUpdateCheckLatest. It only
// tells: menus show the newer release, which the user downloads. The request
// is a plain GET without cookies or anything identifying the user beyond what
// every request carries; failures are retried quietly later. Headless mode and
// --disable-background-networking turn it off.
class UpdateChecker : public ChromeBrowserMainExtraParts {
 public:
  UpdateChecker();
  UpdateChecker(const UpdateChecker&) = delete;
  UpdateChecker& operator=(const UpdateChecker&) = delete;
  ~UpdateChecker() override;

  // ChromeBrowserMainExtraParts:
  void PostBrowserStart() override;
  void PostMainMessageLoopRun() override;

 private:
  void ScheduleCheck(base::TimeDelta delay);
  void Check();
  void OnResponse(std::optional<std::string> body);

  raw_ptr<PrefService> local_state_ = nullptr;
  base::OneShotTimer timer_;
  std::unique_ptr<network::SimpleURLLoader> loader_;
};

}  // namespace alerse

#endif  // CHROME_BROWSER_ALERSE_UPDATE_ALERSE_UPDATE_CHECKER_H_
