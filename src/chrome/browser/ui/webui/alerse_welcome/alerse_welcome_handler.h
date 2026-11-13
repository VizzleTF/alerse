// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ALERSE_WELCOME_ALERSE_WELCOME_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ALERSE_WELCOME_ALERSE_WELCOME_HANDLER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

class Profile;

// The first start's page, chrome://alerse-welcome: the search engine, the
// theme and the default browser, for every Space. Importing is the import
// page's handler, which the page carries too.
class AlerseWelcomeHandler : public content::WebUIMessageHandler {
 public:
  explicit AlerseWelcomeHandler(Profile* profile);
  AlerseWelcomeHandler(const AlerseWelcomeHandler&) = delete;
  AlerseWelcomeHandler& operator=(const AlerseWelcomeHandler&) = delete;
  ~AlerseWelcomeHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  void HandleGetState(const base::ListValue& args);
  void HandleSetSearchEngine(const base::ListValue& args);
  void HandleSetTheme(const base::ListValue& args);
  void HandleSetDefaultBrowser(const base::ListValue& args);

  const raw_ptr<Profile> profile_;
  base::WeakPtrFactory<AlerseWelcomeHandler> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_ALERSE_WELCOME_ALERSE_WELCOME_HANDLER_H_
