// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/alerse_import/alerse_import_ui.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/alerse_import/alerse_import_handler.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"

namespace {

#include "chrome/browser/ui/webui/alerse_common/alerse_page_style.inc"
#include "chrome/browser/ui/webui/alerse_import/alerse_import_page.inc"

// The page's files, by path; "" is the page itself. strings.m.js is the
// data source's own.
std::string_view FindFile(const std::string& path) {
  if (path.empty() || path == "index.html") {
    return kAlerseImportHtml;
  }
  if (path == "base.css") {
    return kAlersePageBaseCss;
  }
  if (path == "alerse_import.css") {
    return kAlerseImportCss;
  }
  if (path == "alerse_import.js") {
    return kAlerseImportJs;
  }
  return {};
}

constexpr webui::LocalizedString kStrings[] = {
    {"title", IDS_ALERSE_IMPORT_TITLE},
    {"lede", IDS_ALERSE_IMPORT_LEDE},
    {"searching", IDS_ALERSE_IMPORT_SEARCHING},
    {"nothingFound", IDS_ALERSE_IMPORT_NOTHING_FOUND},
    {"replaces", IDS_ALERSE_IMPORT_REPLACES},
    {"otherLink", IDS_ALERSE_IMPORT_OTHER_LINK},
    {"otherHint", IDS_ALERSE_IMPORT_OTHER_HINT},
    {"arcSummary", IDS_ALERSE_IMPORT_ARC_SUMMARY},
    {"arcHint", IDS_ALERSE_IMPORT_ARC_HINT},
    {"bookmarksSummary", IDS_ALERSE_IMPORT_BOOKMARKS_SUMMARY},
    {"bookmarksHint", IDS_ALERSE_IMPORT_BOOKMARKS_HINT},
    {"importButton", IDS_ALERSE_IMPORT_BUTTON},
    {"importAgainButton", IDS_ALERSE_IMPORT_AGAIN_BUTTON},
    {"working", IDS_ALERSE_IMPORT_WORKING},
    {"arcDone", IDS_ALERSE_IMPORT_ARC_DONE},
    {"spacesCreated", IDS_ALERSE_IMPORT_SPACES_CREATED},
    {"spacesPending", IDS_ALERSE_IMPORT_SPACES_PENDING},
    {"bookmarksDone", IDS_ALERSE_IMPORT_BOOKMARKS_DONE},
    {"errorBusy", IDS_ALERSE_IMPORT_ERROR_BUSY},
    {"errorNotReady", IDS_ALERSE_IMPORT_ERROR_NOT_READY},
    {"errorRead", IDS_ALERSE_IMPORT_ERROR_READ},
    {"errorUnknown", IDS_ALERSE_IMPORT_ERROR_UNKNOWN},
};

}  // namespace

bool AlerseImportUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return Profile::FromBrowserContext(browser_context)->IsRegularProfile();
}

AlerseImportUI::AlerseImportUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, kChromeUIAlerseImportHost);
  source->AddLocalizedStrings(kStrings);
  source->UseStringsJs();
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
  web_ui->AddMessageHandler(std::make_unique<AlerseImportHandler>(profile));
}

AlerseImportUI::~AlerseImportUI() = default;
