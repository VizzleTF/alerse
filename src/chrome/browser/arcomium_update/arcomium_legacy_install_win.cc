// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/arcomium_update/arcomium_legacy_install_win.h"

#include <windows.h>

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/win/registry.h"
#include "chrome/common/chrome_paths.h"

namespace arcomium {

namespace {

// Arcomium's uninstall entry, as its installer wrote it (its base_app_id).
constexpr wchar_t kUninstallKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Arcomium";
// For tests: another HKCU key to read the entry from, so a test never meets
// the Arcomium the user has installed.
constexpr char kUninstallKeySwitch[] = "alerse-legacy-uninstall-key";

constexpr base::TimeDelta kDelay = base::Minutes(1);

bool SameName(const base::FilePath& path, const wchar_t* name) {
  return base::FilePath::CompareEqualIgnoreCase(path.BaseName().value(),
                                                name);
}

void RemoveLegacyInstall(std::wstring key_name) {
  base::win::RegKey key;
  if (key.Open(HKEY_CURRENT_USER, key_name.c_str(), KEY_QUERY_VALUE) !=
      ERROR_SUCCESS) {
    return;
  }
  std::wstring uninstall_string;
  if (key.ReadValue(L"UninstallString", &uninstall_string) != ERROR_SUCCESS) {
    return;
  }
  base::FilePath exe;
  base::FilePath user_data_dir;
  if (!base::PathService::Get(base::FILE_EXE, &exe) ||
      !base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
    return;
  }
  const base::FilePath base = exe.DirName().DirName().DirName();
  const bool has_old_profile = base::PathExists(
      base.Append(L"Arcomium").Append(L"User Data").Append(L"Local State"));
  std::optional<base::CommandLine> command = GetLegacyUninstallCommand(
      uninstall_string, exe, user_data_dir, has_old_profile);
  if (!command) {
    LOG(WARNING) << "Alerse: Arcomium's installation (" << uninstall_string
                 << ") is left alone: it is not beside this Alerse "
                 << exe.value() << " (profile " << user_data_dir.value()
                 << "), or its profile was not taken over";
    return;
  }
  LOG(WARNING) << "Alerse: removing Arcomium, its profile taken over: "
               << command->GetCommandLineString();
  base::LaunchOptions options;
  options.start_hidden = true;
  const base::Process process = base::LaunchProcess(*command, options);
  LOG_IF(WARNING, !process.IsValid())
      << "Alerse: Arcomium's uninstaller did not start";
}

}  // namespace

std::optional<base::CommandLine> GetLegacyUninstallCommand(
    const std::wstring& uninstall_string,
    const base::FilePath& exe,
    const base::FilePath& user_data_dir,
    bool has_old_profile) {
  if (has_old_profile || uninstall_string.empty()) {
    return std::nullopt;
  }
  // BASE\Alerse\Application\chrome.exe with BASE\Alerse\User Data.
  const base::FilePath application = exe.DirName();
  const base::FilePath product = application.DirName();
  if (!SameName(exe, L"chrome.exe") || !SameName(application, L"Application") ||
      !SameName(product, L"Alerse") ||
      !base::FilePath::CompareEqualIgnoreCase(
          user_data_dir.value(), product.Append(L"User Data").value())) {
    return std::nullopt;
  }
  const base::FilePath base = product.DirName();
  // BASE\Arcomium\Application\VERSION\Installer\setup.exe --uninstall.
  base::CommandLine command = base::CommandLine::FromString(uninstall_string);
  const base::FilePath setup = command.GetProgram();
  const base::FilePath installer = setup.DirName();
  if (!SameName(setup, L"setup.exe") || !SameName(installer, L"Installer") ||
      !base::FilePath::CompareEqualIgnoreCase(
          installer.DirName().DirName().value(),
          base.Append(L"Arcomium").Append(L"Application").value()) ||
      !command.HasSwitch("uninstall") || command.HasSwitch("system-level")) {
    return std::nullopt;
  }
  // Silently, without asking about user data, which stays.
  command.AppendSwitch("force-uninstall");
  return command;
}

void RemoveLegacyInstallLater() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  std::wstring key = kUninstallKey;
  if (command_line.HasSwitch(kUninstallKeySwitch)) {
    key = command_line.GetSwitchValueNative(kUninstallKeySwitch);
  }
  base::ThreadPool::PostDelayedTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&RemoveLegacyInstall, std::move(key)), kDelay);
}

}  // namespace arcomium
