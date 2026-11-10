// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/alerse_update/alerse_legacy_install_win.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace alerse {

namespace {

const base::FilePath kExe(
    L"C:\\Users\\u\\AppData\\Local\\Alerse\\Application\\chrome.exe");
const base::FilePath kUserData(
    L"C:\\Users\\u\\AppData\\Local\\Alerse\\User Data");
constexpr wchar_t kUninstall[] =
    L"\"C:\\Users\\u\\AppData\\Local\\Arcomium\\Application\\157.0.8103.3807"
    L"\\Installer\\setup.exe\" --uninstall";

}  // namespace

TEST(AlerseLegacyInstallTest, UninstallsArcomiumBesideAlerse) {
  std::optional<base::CommandLine> command =
      GetLegacyUninstallCommand(kUninstall, kExe, kUserData, false);
  ASSERT_TRUE(command);
  EXPECT_EQ(command->GetProgram().value(),
            L"C:\\Users\\u\\AppData\\Local\\Arcomium\\Application\\"
            L"157.0.8103.3807\\Installer\\setup.exe");
  EXPECT_TRUE(command->HasSwitch("uninstall"));
  EXPECT_TRUE(command->HasSwitch("force-uninstall"));
  // Case does not matter in Windows paths.
  EXPECT_TRUE(GetLegacyUninstallCommand(
      kUninstall,
      base::FilePath(
          L"c:\\users\\U\\appdata\\local\\alerse\\application\\Chrome.exe"),
      kUserData, false));
}

TEST(AlerseLegacyInstallTest, LeavesArcomiumWithItsProfile) {
  EXPECT_FALSE(GetLegacyUninstallCommand(kUninstall, kExe, kUserData, true));
}

TEST(AlerseLegacyInstallTest, OnlyFromAnInstalledAlerseWithItsProfile) {
  // A development build.
  EXPECT_FALSE(GetLegacyUninstallCommand(
      kUninstall, base::FilePath(L"C:\\src\\out\\Default\\chrome.exe"),
      kUserData, false));
  // --user-data-dir.
  EXPECT_FALSE(GetLegacyUninstallCommand(
      kUninstall, kExe, base::FilePath(L"C:\\tmp\\profile"), false));
  // Another folder than Arcomium's beside it.
  EXPECT_FALSE(GetLegacyUninstallCommand(
      kUninstall,
      base::FilePath(L"D:\\Apps\\Alerse\\Application\\chrome.exe"),
      base::FilePath(L"D:\\Apps\\Alerse\\User Data"), false));
}

TEST(AlerseLegacyInstallTest, OnlyArcomiumsOwnPerUserUninstaller) {
  EXPECT_FALSE(GetLegacyUninstallCommand(L"", kExe, kUserData, false));
  EXPECT_FALSE(GetLegacyUninstallCommand(
      L"\"C:\\Users\\u\\AppData\\Local\\Arcomium\\Application\\1.0\\Installer"
      L"\\other.exe\" --uninstall",
      kExe, kUserData, false));
  EXPECT_FALSE(GetLegacyUninstallCommand(
      L"\"C:\\Users\\u\\AppData\\Local\\Arcomium\\Application\\1.0\\Installer"
      L"\\setup.exe\"",
      kExe, kUserData, false));
  EXPECT_FALSE(GetLegacyUninstallCommand(
      std::wstring(kUninstall) + L" --system-level", kExe, kUserData, false));
  EXPECT_FALSE(GetLegacyUninstallCommand(
      L"\"C:\\Program Files\\Arcomium\\Application\\1.0\\Installer\\setup.exe\""
      L" --uninstall",
      kExe, kUserData, false));
}

}  // namespace alerse
