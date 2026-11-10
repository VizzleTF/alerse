// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ALERSE_IMPORT_ALERSE_IMPORT_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ALERSE_IMPORT_ALERSE_IMPORT_HANDLER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/alerse_import/alerse_import_data.h"
#include "content/public/browser/web_ui_message_handler.h"

class Profile;

// Backs chrome://alerse-import. The page calls:
//
//   getSources()          -> [{id, kind, browser, profile, links, spaces}]
//   importSource(id)      -> {links, folders, spaces, createdSpaces, folder}
//
// importSource() takes only an id that the last getSources() returned. It
// rejects with "busy", "notReady" (bookmarks still loading) or "readFailed".
class AlerseImportHandler : public content::WebUIMessageHandler {
 public:
  explicit AlerseImportHandler(Profile* profile);
  AlerseImportHandler(const AlerseImportHandler&) = delete;
  AlerseImportHandler& operator=(const AlerseImportHandler&) = delete;
  ~AlerseImportHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptDisallowed() override;

 private:
  void HandleGetSources(const base::ListValue& args);
  void HandleImportSource(const base::ListValue& args);

  void OnSourcesFound(const std::string& callback_id,
                      std::vector<alerse_import::Source> sources);
  void OnArcRead(const std::string& callback_id,
                 std::optional<std::vector<alerse_import::ArcSpace>> spaces);
  void OnBookmarksRead(const std::string& callback_id,
                       const alerse_import::Source& source,
                       std::optional<alerse_import::Node> root);

  void Reject(const std::string& callback_id, const std::string& error);

  const raw_ptr<Profile> profile_;
  // What the last getSources() found; the only ids importSource() takes.
  std::vector<alerse_import::Source> sources_;
  bool importing_ = false;
  base::WeakPtrFactory<AlerseImportHandler> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_ALERSE_IMPORT_ALERSE_IMPORT_HANDLER_H_
