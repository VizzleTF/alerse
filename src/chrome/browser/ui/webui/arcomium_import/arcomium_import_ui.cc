// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/arcomium_import/arcomium_import_ui.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/arcomium_import/arcomium_import_handler.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"

namespace {

#include "chrome/browser/ui/webui/arcomium_common/arcomium_page_style.inc"
#include "chrome/browser/ui/webui/arcomium_import/arcomium_import_page.inc"

// The page's files, by path; "" is the page itself. strings.m.js is the
// data source's own.
std::string_view FindFile(const std::string& path) {
  if (path.empty() || path == "index.html") {
    return kArcomiumImportHtml;
  }
  if (path == "base.css") {
    return kArcomiumPageBaseCss;
  }
  if (path == "arcomium_import.css") {
    return kArcomiumImportCss;
  }
  if (path == "arcomium_import.js") {
    return kArcomiumImportJs;
  }
  return {};
}

constexpr webui::LocalizedString kStrings[] = {
    {"title", IDS_ARCOMIUM_IMPORT_TITLE},
    {"lede", IDS_ARCOMIUM_IMPORT_LEDE},
    {"searching", IDS_ARCOMIUM_IMPORT_SEARCHING},
    {"nothingFound", IDS_ARCOMIUM_IMPORT_NOTHING_FOUND},
    {"replaces", IDS_ARCOMIUM_IMPORT_REPLACES},
    {"otherLink", IDS_ARCOMIUM_IMPORT_OTHER_LINK},
    {"otherHint", IDS_ARCOMIUM_IMPORT_OTHER_HINT},
    {"arcSummary", IDS_ARCOMIUM_IMPORT_ARC_SUMMARY},
    {"arcHint", IDS_ARCOMIUM_IMPORT_ARC_HINT},
    {"bookmarksSummary", IDS_ARCOMIUM_IMPORT_BOOKMARKS_SUMMARY},
    {"bookmarksHint", IDS_ARCOMIUM_IMPORT_BOOKMARKS_HINT},
    {"importButton", IDS_ARCOMIUM_IMPORT_BUTTON},
    {"importAgainButton", IDS_ARCOMIUM_IMPORT_AGAIN_BUTTON},
    {"working", IDS_ARCOMIUM_IMPORT_WORKING},
    {"arcDone", IDS_ARCOMIUM_IMPORT_ARC_DONE},
    {"spacesCreated", IDS_ARCOMIUM_IMPORT_SPACES_CREATED},
    {"spacesPending", IDS_ARCOMIUM_IMPORT_SPACES_PENDING},
    {"bookmarksDone", IDS_ARCOMIUM_IMPORT_BOOKMARKS_DONE},
    {"errorBusy", IDS_ARCOMIUM_IMPORT_ERROR_BUSY},
    {"errorNotReady", IDS_ARCOMIUM_IMPORT_ERROR_NOT_READY},
    {"errorRead", IDS_ARCOMIUM_IMPORT_ERROR_READ},
    {"errorUnknown", IDS_ARCOMIUM_IMPORT_ERROR_UNKNOWN},
};

}  // namespace

bool ArcomiumImportUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return Profile::FromBrowserContext(browser_context)->IsRegularProfile();
}

ArcomiumImportUI::ArcomiumImportUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, kChromeUIArcomiumImportHost);
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
  web_ui->AddMessageHandler(std::make_unique<ArcomiumImportHandler>(profile));
}

ArcomiumImportUI::~ArcomiumImportUI() = default;
