// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/alerse_update/alerse_update_checker.h"

#include <string>
#include <string_view>

#include "build/build_config.h"
#include "chrome/browser/alerse_update/alerse_version.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace alerse {
namespace {

TEST(AlerseUpdateCheckerTest, ParsesReleaseTags) {
  EXPECT_EQ(ParseReleaseTag("v0.1.5"), base::Version("0.1.5"));
  EXPECT_EQ(ParseReleaseTag("0.1.5"), base::Version("0.1.5"));
  EXPECT_EQ(ParseReleaseTag("v10.20.30"), base::Version("10.20.30"));

  EXPECT_FALSE(ParseReleaseTag(""));
  EXPECT_FALSE(ParseReleaseTag("v"));
  EXPECT_FALSE(ParseReleaseTag("v0.1"));
  EXPECT_FALSE(ParseReleaseTag("v0.1.5.1"));
  EXPECT_FALSE(ParseReleaseTag("v0.1.5-beta"));
  EXPECT_FALSE(ParseReleaseTag("V0.1.5"));
  EXPECT_FALSE(ParseReleaseTag("vv0.1.5"));
  EXPECT_FALSE(ParseReleaseTag("v0..5"));
  EXPECT_FALSE(ParseReleaseTag("v-1.1.5"));
  EXPECT_FALSE(ParseReleaseTag("v 0.1.5"));
}

TEST(AlerseUpdateCheckerTest, ComparesVersionsByNumber) {
  EXPECT_TRUE(IsNewerRelease("0.1.5", "0.1.4"));
  EXPECT_TRUE(IsNewerRelease("0.1.10", "0.1.9"));
  EXPECT_TRUE(IsNewerRelease("0.2.0", "0.1.99"));
  EXPECT_TRUE(IsNewerRelease("1.0.0", "0.9.9"));

  EXPECT_FALSE(IsNewerRelease("0.1.4", "0.1.4"));
  EXPECT_FALSE(IsNewerRelease("0.1.3", "0.1.4"));
  EXPECT_FALSE(IsNewerRelease("0.1.9", "0.1.10"));
  EXPECT_FALSE(IsNewerRelease("0.1.99", "0.2.0"));
}

TEST(AlerseUpdateCheckerTest, IgnoresMalformedVersions) {
  EXPECT_FALSE(IsNewerRelease("", "0.1.4"));
  EXPECT_FALSE(IsNewerRelease("0.1.5", ""));
  EXPECT_FALSE(IsNewerRelease("v0.1.5", "0.1.4"));
  EXPECT_FALSE(IsNewerRelease("0.2", "0.1.4"));
  EXPECT_FALSE(IsNewerRelease("garbage", "0.1.4"));
}

TEST(AlerseUpdateCheckerTest, TellsAnswersFromFailures) {
  // An answer, even one without a release, waits a day.
  EXPECT_TRUE(IsAnswer(200));
  EXPECT_TRUE(IsAnswer(404));
  // No response, rate limits and server errors are retried sooner.
  EXPECT_FALSE(IsAnswer(0));
  EXPECT_FALSE(IsAnswer(403));
  EXPECT_FALSE(IsAnswer(429));
  EXPECT_FALSE(IsAnswer(500));
  EXPECT_FALSE(IsAnswer(503));
}

TEST(AlerseUpdateCheckerTest, ReleaseUrls) {
#if BUILDFLAG(IS_WIN)
  constexpr std::string_view kReleases =
      "https://github.com/VizzleTF/alerse-win/releases/";
#elif BUILDFLAG(IS_MAC)
  constexpr std::string_view kReleases =
      "https://github.com/VizzleTF/alerse-mac/releases/";
#else
  constexpr std::string_view kReleases =
      "https://github.com/VizzleTF/alerse-linux/releases/";
#endif
  EXPECT_EQ(GetLatestReleaseUrl().spec(), std::string(kReleases) + "latest");
  EXPECT_EQ(GetReleaseUrl("0.1.5").spec(),
            std::string(kReleases) + "tag/v0.1.5");
}

TEST(AlerseUpdateCheckerTest, AvailableUpdate) {
  if (!ParseReleaseTag(ALERSE_VERSION)) {
    GTEST_SKIP() << "This build does not know its Alerse version.";
  }
  TestingPrefServiceSimple local_state;
  RegisterUpdateCheckPrefs(local_state.registry());
  EXPECT_EQ(GetAvailableUpdate(&local_state), "");
  EXPECT_EQ(GetAvailableUpdate(nullptr), "");

  local_state.SetString(kUpdateCheckLatest, ALERSE_VERSION);
  EXPECT_EQ(GetAvailableUpdate(&local_state), "");

  local_state.SetString(kUpdateCheckLatest, "0.0.0");
  EXPECT_EQ(GetAvailableUpdate(&local_state), "");

  local_state.SetString(kUpdateCheckLatest, "999.0.0");
  EXPECT_EQ(GetAvailableUpdate(&local_state), "999.0.0");

  // Turning checks off hides what an earlier check found.
  local_state.SetBoolean(kUpdateCheckEnabled, false);
  EXPECT_EQ(GetAvailableUpdate(&local_state), "");
}

}  // namespace
}  // namespace alerse
