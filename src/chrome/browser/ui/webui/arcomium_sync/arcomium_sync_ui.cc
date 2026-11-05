// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/arcomium_sync/arcomium_sync_ui.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/arcomium_sync/arcomium_sync_handler.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace {

#include "chrome/browser/ui/webui/arcomium_common/arcomium_page_style.inc"
#include "chrome/browser/ui/webui/arcomium_sync/arcomium_sync_page.inc"

// The page's files, by path; "" is the page itself.
std::string_view FindFile(const std::string& path) {
  if (path.empty() || path == "index.html") {
    return kArcomiumSyncHtml;
  }
  if (path == "base.css") {
    return kArcomiumPageBaseCss;
  }
  if (path == "arcomium_sync.css") {
    return kArcomiumSyncCss;
  }
  if (path == "arcomium_sync.js") {
    return kArcomiumSyncJs;
  }
  return {};
}

}  // namespace

ArcomiumSyncUI::ArcomiumSyncUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, kChromeUIArcomiumSyncHost);
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
  web_ui->AddMessageHandler(std::make_unique<ArcomiumSyncHandler>(profile));
}

ArcomiumSyncUI::~ArcomiumSyncUI() = default;
