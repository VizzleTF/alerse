// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ARCOMIUM_UPDATE_ARCOMIUM_LEGACY_INSTALL_WIN_H_
#define CHROME_BROWSER_ARCOMIUM_UPDATE_ARCOMIUM_LEGACY_INSTALL_WIN_H_

#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"

namespace arcomium {

// Alerse was called Arcomium up to 0.1.7 and installs beside it, with its own
// identity (defaults/own-install-identity.patch). On its first start Alerse
// takes over Arcomium's profile; after that the old installation is only a
// second browser without data, so Alerse removes it with Arcomium's own
// uninstaller, silently (--force-uninstall, which keeps user data): its
// shortcuts, taskbar pins, uninstall entry, ProgIDs and files go. Nothing
// happens unless all of these hold, BASE being the folder that holds this
// Alerse (%LOCALAPPDATA% for a per-user install):
// - this Alerse runs as installed, BASE\Alerse\Application\chrome.exe, with
//   its default profile BASE\Alerse\User Data (never with --user-data-dir);
// - Arcomium's uninstall entry (HKCU) runs BASE\Arcomium\Application\
//   VERSION\Installer\setup.exe --uninstall, not a system-level one;
// - Arcomium's profile was taken over: BASE\Arcomium\User Data has no Local
//   State (while Arcomium runs, it keeps its profile, and so its
//   installation, until a later start).
// It runs a minute after startup, on a background thread, and again on every
// start until the entry is gone.
void RemoveLegacyInstallLater();

// The command that uninstalls Arcomium, or nullopt when the conditions above
// do not hold. `uninstall_string` is the UninstallString of Arcomium's entry,
// `exe` this browser's chrome.exe, `user_data_dir` its profile directory;
// `has_old_profile` tells whether Arcomium's User Data still has a profile.
std::optional<base::CommandLine> GetLegacyUninstallCommand(
    const std::wstring& uninstall_string,
    const base::FilePath& exe,
    const base::FilePath& user_data_dir,
    bool has_old_profile);

}  // namespace arcomium

#endif  // CHROME_BROWSER_ARCOMIUM_UPDATE_ARCOMIUM_LEGACY_INSTALL_WIN_H_
