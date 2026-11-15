// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ALERSE_IMPORT_ALERSE_IMPORT_ARC_DATA_H_
#define CHROME_BROWSER_UI_WEBUI_ALERSE_IMPORT_ALERSE_IMPORT_ARC_DATA_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "url/gurl.h"

// Arc's browsing history and saved passwords, read from its own profile
// ("User Data/Default" in Arc's directory). Arc encrypts passwords as
// Chromium does: on Windows with a key that DPAPI keeps for this Windows user,
// on macOS with a key from the Keychain entry "Arc Safe Storage", which macOS
// asks the user to allow.

namespace alerse_import {

struct ArcPage {
  GURL url;
  std::u16string title;
  int visit_count = 0;
  int typed_count = 0;
  base::Time last_visit;
};

struct ArcLogin {
  ArcLogin();
  ArcLogin(ArcLogin&&);
  ArcLogin& operator=(ArcLogin&&);
  ~ArcLogin();

  GURL url;
  GURL action;
  std::string signon_realm;
  std::u16string username_element;
  std::u16string username;
  std::u16string password_element;
  std::u16string password;
  base::Time created;
  // A site the user told Arc never to save a password for.
  bool blocked = false;
  // Chromium's PasswordForm::Scheme: 0 a web form, else HTTP authentication.
  int scheme = 0;
};

struct ArcData {
  ArcData();
  ArcData(ArcData&&);
  ArcData& operator=(ArcData&&);
  ~ArcData();

  std::vector<ArcPage> pages;
  std::vector<ArcLogin> logins;
  // Saved passwords that could not be decrypted (no key, or another format).
  int locked = 0;
};

// The most recent pages taken from Arc's history.
inline constexpr size_t kMaxArcPages = 100000;

// Reads Arc's history and passwords from copies of its databases, so a
// running Arc keeps its own. Asks for the key only when there is a password
// to decrypt. Blocks.
ArcData ReadArcData(const base::FilePath& arc_dir);

}  // namespace alerse_import

#endif  // CHROME_BROWSER_UI_WEBUI_ALERSE_IMPORT_ALERSE_IMPORT_ARC_DATA_H_
