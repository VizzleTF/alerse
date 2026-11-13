// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/alerse_webkit/alerse_webkit_ui.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/referrer.h"
#include "ui/base/page_transition_types.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/webui/alerse_webkit/alerse_webkit_host_mac.h"
#endif

namespace {

#include "chrome/browser/ui/webui/alerse_common/alerse_page_style.inc"
#include "chrome/browser/ui/webui/alerse_webkit/alerse_webkit_page.inc"

std::string_view FindFile(const std::string& path) {
  if (path.empty() || path == "index.html" || path.starts_with("?")) {
    return kAlerseWebKitHtml;
  }
  if (path == "base.css") {
    return kAlersePageBaseCss;
  }
  if (path == "alerse_webkit.css") {
    return kAlerseWebKitCss;
  }
  if (path == "alerse_webkit.js") {
    return kAlerseWebKitJs;
  }
  return {};
}

// The bar's height on macOS, in points: the WebKit view is below it.
constexpr int kBarHeight = 40;

const char* Platform() {
#if BUILDFLAG(IS_MAC)
  return "mac";
#elif BUILDFLAG(IS_WIN)
  return "win";
#else
  return "linux";
#endif
}

class AlerseWebKitHandler : public content::WebUIMessageHandler {
 public:
  AlerseWebKitHandler() = default;
  ~AlerseWebKitHandler() override = default;

  void RegisterMessages() override {
    auto add = [this](const char* name,
                      void (AlerseWebKitHandler::*method)(
                          const base::ListValue&)) {
      web_ui()->RegisterMessageCallback(
          name, base::BindRepeating(method, base::Unretained(this)));
    };
    add("getWebKitState", &AlerseWebKitHandler::HandleGetState);
    add("startWebKit", &AlerseWebKitHandler::HandleStart);
    add("webKitBack", &AlerseWebKitHandler::HandleBack);
    add("webKitReload", &AlerseWebKitHandler::HandleReload);
    add("openInChromium", &AlerseWebKitHandler::HandleOpenInChromium);
    add("openElsewhere", &AlerseWebKitHandler::HandleOpenElsewhere);
  }

  void OnJavascriptDisallowed() override {
#if BUILDFLAG(IS_MAC)
    host_.reset();
#endif
    weak_factory_.InvalidateWeakPtrs();
  }

 private:
  content::WebContents* contents() { return web_ui()->GetWebContents(); }

  // The site: the one WebKit shows now, else the page's.
  GURL Site() {
    return current_.is_valid()
               ? current_
               : alerse::SiteOfWebKitPage(contents()->GetLastCommittedURL());
  }

  void HandleGetState(const base::ListValue& args) {
    AllowJavascript();
    const GURL site = Site();
    ResolveJavascriptCallback(args[0],
                              base::DictValue()
                                  .Set("site", site.spec())
                                  .Set("host", site.host())
                                  .Set("platform", Platform())
                                  .Set("barHeight", kBarHeight));
  }

  void HandleStart(const base::ListValue& args) {
    AllowJavascript();
#if BUILDFLAG(IS_MAC)
    const GURL site = Site();
    if (host_ || !site.is_valid()) {
      return;
    }
    host_ = std::make_unique<AlerseWebKitHost>(
        contents(), site, kBarHeight,
        base::BindRepeating(&AlerseWebKitHandler::OnWebKitChanged,
                            weak_factory_.GetWeakPtr()));
#endif
  }

  void HandleBack(const base::ListValue& args) {
#if BUILDFLAG(IS_MAC)
    if (host_) {
      host_->GoBack();
    }
#endif
  }

  void HandleReload(const base::ListValue& args) {
#if BUILDFLAG(IS_MAC)
    if (host_) {
      host_->Reload();
    }
#endif
  }

  // The user's choice: the site opens in Chromium in this tab from now on.
  void HandleOpenInChromium(const base::ListValue& args) {
    const GURL site = Site();
    if (!site.is_valid()) {
      return;
    }
    alerse::AllowInChromium(contents(), site);
    contents()->GetController().LoadURL(site, content::Referrer(),
                                        ui::PAGE_TRANSITION_LINK,
                                        std::string());
  }

  // Microsoft Edge on Windows and Safari on macOS carry their makers' DRM.
  void HandleOpenElsewhere(const base::ListValue& args) {
    const GURL site = Site();
    if (!site.is_valid()) {
      return;
    }
#if BUILDFLAG(IS_WIN)
    platform_util::OpenExternal(GURL("microsoft-edge:" + site.spec()));
#elif BUILDFLAG(IS_MAC)
    AlerseOpenInSafari(site);
#endif
  }

#if BUILDFLAG(IS_MAC)
  void OnWebKitChanged(const AlerseWebKitHost::State& state) {
    if (state.url.is_valid()) {
      current_ = state.url;
    }
    FireWebUIListener("webkit-state", base::DictValue()
                                          .Set("title", state.title)
                                          .Set("site", current_.spec())
                                          .Set("host", current_.host())
                                          .Set("canGoBack", state.can_go_back)
                                          .Set("loading", state.loading));
  }

  std::unique_ptr<AlerseWebKitHost> host_;
#endif
  GURL current_;
  base::WeakPtrFactory<AlerseWebKitHandler> weak_factory_{this};
};

}  // namespace

AlerseWebKitUI::AlerseWebKitUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), alerse::kChromeUIAlerseWebKitHost);
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
  web_ui->AddMessageHandler(std::make_unique<AlerseWebKitHandler>());
}

AlerseWebKitUI::~AlerseWebKitUI() = default;
