// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/alerse_welcome/alerse_welcome_ui.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/alerse_import/alerse_import_handler.h"
#include "chrome/browser/ui/webui/alerse_welcome/alerse_welcome_handler.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace {

#include "chrome/browser/ui/webui/alerse_common/alerse_page_style.inc"
#include "chrome/browser/ui/webui/alerse_welcome/alerse_welcome_page.inc"

std::string_view FindFile(const std::string& path) {
  if (path.empty() || path == "index.html") {
    return kAlerseWelcomeHtml;
  }
  if (path == "base.css") {
    return kAlersePageBaseCss;
  }
  if (path == "alerse_welcome.css") {
    return kAlerseWelcomeCss;
  }
  if (path == "alerse_welcome.js") {
    return kAlerseWelcomeJs;
  }
  return {};
}

}  // namespace

bool AlerseWelcomeUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return Profile::FromBrowserContext(browser_context)->IsRegularProfile();
}

AlerseWelcomeUI::AlerseWelcomeUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, kChromeUIAlerseWelcomeHost);
  // The files are compiled in, so the page needs no resource bundle.
  source->SetRequestFilter(
      base::BindRepeating(
          [](const std::string& path) { return !FindFile(path).empty(); }),
      base::BindRepeating(
          [](const std::string& path,
             content::WebUIDataSource::GotDataCallback callback) {
            std::move(callback).Run(
                base::MakeRefCounted<base::RefCountedString>(
                    std::string(FindFile(path))));
          }));
  web_ui->AddMessageHandler(std::make_unique<AlerseWelcomeHandler>(profile));
  // The import step is the import page's own: the same sources and import.
  web_ui->AddMessageHandler(std::make_unique<AlerseImportHandler>(profile));
}

AlerseWelcomeUI::~AlerseWelcomeUI() = default;
