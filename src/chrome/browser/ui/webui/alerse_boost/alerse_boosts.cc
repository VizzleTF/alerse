// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/alerse_boost/alerse_boosts.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/alerse_boost.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

namespace alerse {

namespace {

// The fonts the panel offers, by the name it stores.
std::string_view FontStack(std::string_view font) {
  if (font == "sans") {
    return "system-ui, -apple-system, \"Segoe UI\", Roboto, sans-serif";
  }
  if (font == "serif") {
    return "Charter, Georgia, \"Times New Roman\", serif";
  }
  if (font == "mono") {
    return "ui-monospace, Menlo, Consolas, \"Courier New\", monospace";
  }
  if (font == "rounded") {
    return "ui-rounded, \"SF Pro Rounded\", \"Arial Rounded MT Bold\", "
           "\"Segoe UI\", sans-serif";
  }
  return {};
}

// Puts the boost's sheet on the page, from an isolated world: a constructed
// sheet, which pages' Content-Security-Policy does not refuse as it does a
// <style> element. The sheet object is the world's, one per document.
constexpr char kApplyScript[] = R"JS((css => {
  const d = document;
  let sheet = window.__alerseBoost;
  if (!sheet) {
    if (!css) return;
    sheet = window.__alerseBoost = new CSSStyleSheet();
  }
  sheet.replaceSync(css);
  if (!d.adoptedStyleSheets.includes(sheet)) {
    d.adoptedStyleSheets = [...d.adoptedStyleSheets, sheet];
  }
}))JS";

// Zap's picker: a red box follows the element under the pointer; a click
// takes it, Esc gives up. The page gets neither. The result waits in the
// world's __alerseZapResult for the browser to read.
constexpr char kZapScript[] = R"JS((() => {
  if (window.__alerseZapping) return;
  window.__alerseZapping = true;
  window.__alerseZapResult = null;
  const esc = CSS.escape;
  const box = document.createElement('div');
  box.style.cssText = 'position:fixed;z-index:2147483647;pointer-events:none;' +
      'background:rgba(255,72,72,.22);outline:2px solid #ff4848;' +
      'border-radius:4px;display:none;transition:all 70ms ease-out';
  document.documentElement.appendChild(box);
  const prev = document.documentElement.style.cursor;
  document.documentElement.style.cursor = 'crosshair';
  let current = null;
  const selectorOf = el => {
    const parts = [];
    for (let e = el; e && e.nodeType === 1 && e !== document.documentElement;
         e = e.parentElement) {
      if (e.id && !/\d{4,}/.test(e.id) &&
          document.querySelectorAll('#' + esc(e.id)).length === 1) {
        parts.unshift('#' + esc(e.id));
        break;
      }
      let part = e.localName;
      const classes = [...e.classList]
          .filter(c => !/\d{4,}|hover|focus|active|open|selected/i.test(c))
          .slice(0, 2);
      if (classes.length) part += '.' + classes.map(esc).join('.');
      const parent = e.parentElement;
      if (parent) {
        const same = [...parent.children].filter(c => c.localName === e.localName);
        if (same.length > 1) part += ':nth-of-type(' + (same.indexOf(e) + 1) + ')';
      }
      parts.unshift(part);
      if (e.localName === 'body') break;
    }
    return parts.join(' > ');
  };
  const swallow = e => {
    e.preventDefault(); e.stopPropagation(); e.stopImmediatePropagation();
  };
  const move = e => {
    const el = document.elementFromPoint(e.clientX, e.clientY);
    if (!el || el === box || el === document.documentElement ||
        el === document.body) {
      current = null; box.style.display = 'none'; return;
    }
    current = el;
    const r = el.getBoundingClientRect();
    Object.assign(box.style, {display: 'block', left: r.left + 'px',
        top: r.top + 'px', width: r.width + 'px', height: r.height + 'px'});
  };
  const finish = result => {
    removeEventListener('mousemove', move, true);
    for (const t of ['mousedown', 'mouseup', 'pointerdown', 'pointerup',
                     'auxclick', 'contextmenu'])
      removeEventListener(t, swallow, true);
    removeEventListener('click', click, true);
    removeEventListener('keydown', key, true);
    box.remove();
    document.documentElement.style.cursor = prev;
    window.__alerseZapping = false;
    window.__alerseZapResult = result;
  };
  const click = e => {
    swallow(e);
    move(e);
    finish(current ? selectorOf(current) : '');
  };
  const key = e => { if (e.key === 'Escape') { swallow(e); finish(''); } };
  addEventListener('mousemove', move, true);
  for (const t of ['mousedown', 'mouseup', 'pointerdown', 'pointerup',
                   'auxclick', 'contextmenu'])
    addEventListener(t, swallow, true);
  addEventListener('click', click, true);
  addEventListener('keydown', key, true);
})())JS";

constexpr char kZapCancelScript[] = R"JS((() => {
  if (window.__alerseZapping) {
    dispatchEvent(new KeyboardEvent('keydown', {key: 'Escape'}));
  }
})())JS";

PrefService* PrefsOf(content::WebContents* contents) {
  return Profile::FromBrowserContext(contents->GetBrowserContext())
      ->GetPrefs();
}

}  // namespace

std::string BoostSiteOf(const GURL& url) {
  if (!url.SchemeIsHTTPOrHTTPS() || url.host().empty()) {
    return std::string();
  }
  std::string site = net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  return site.empty() ? std::string(url.host()) : site;
}

std::string BoostCss(const base::DictValue& boost) {
  if (!boost.FindBool("enabled").value_or(true)) {
    return std::string();
  }
  std::string css;
  // Color: the page's colors turned round the wheel, and darkened by
  // inverting its lightness. Pictures and video get the opposite filter
  // first, so they keep their own colors.
  const int hue = boost.FindInt("hue").value_or(-1);
  const bool dark = boost.FindBool("dark").value_or(false);
  std::string page_filter;
  std::string undo_filter;
  if (dark) {
    page_filter = "invert(1) hue-rotate(180deg)";
    undo_filter = "hue-rotate(180deg) invert(1)";
  }
  if (hue > 0 && hue < 360) {
    const std::string deg = base::NumberToString(hue);
    page_filter += (page_filter.empty() ? "" : " ") +
                   std::string("hue-rotate(") + deg + "deg)";
    undo_filter = "hue-rotate(-" + deg + "deg)" +
                  (undo_filter.empty() ? "" : " ") + undo_filter;
  }
  if (!page_filter.empty()) {
    css += "html{filter:" + page_filter + "!important}";
    css += "img,video,picture,canvas,iframe,embed,object,"
           "[style*=\"background-image\"]{filter:" +
           undo_filter + "!important}";
  }
  if (dark) {
    css += "html{background:#fff!important}";
  }
  // Font: every text but code and icon fonts.
  if (const std::string* font = boost.FindString("font")) {
    const std::string_view stack = FontStack(*font);
    if (!stack.empty()) {
      css += "body,body :not(i):not(code):not(pre):not(pre *):not(kbd)"
             ":not(samp):not(svg *):not([class*=\"icon\" i])"
             ":not([class*=\"symbol\" i]):not([class*=\"fa-\"]){font-family:" +
             std::string(stack) + "!important}";
    }
  }
  const int size = boost.FindInt("size").value_or(100);
  if (size >= 50 && size <= 200 && size != 100) {
    css += "html{zoom:" + base::NumberToString(size) + "%}";
  }
  // Zapped elements: a rule each, so that one a page no longer matches
  // cannot take the others with it.
  if (const base::ListValue* zaps = boost.FindList("zaps")) {
    for (const base::Value& zap : *zaps) {
      if (zap.is_string() && !zap.GetString().empty() &&
          zap.GetString().size() < 2000) {
        css += zap.GetString() + "{display:none!important}";
      }
    }
  }
  if (const std::string* own = boost.FindString("css")) {
    css += "\n" + *own;
  }
  return css;
}

const base::DictValue* FindBoost(PrefService* prefs, const std::string& site) {
  if (site.empty()) {
    return nullptr;
  }
  return prefs->GetDict(kBoostsPref).FindDict(site);
}

void SetBoost(PrefService* prefs, const std::string& site,
              base::DictValue boost) {
  if (site.empty()) {
    return;
  }
  ScopedDictPrefUpdate update(prefs, kBoostsPref);
  if (boost.empty()) {
    update->Remove(site);
  } else {
    update->Set(site, std::move(boost));
  }
}

void MaybeAttachBoosts(content::NavigationThrottleRegistry& registry) {
  content::NavigationHandle& handle = registry.GetNavigationHandle();
  if (handle.IsInPrimaryMainFrame()) {
    BoostTabHelper::CreateForWebContents(handle.GetWebContents());
  }
}

BoostTabHelper::BoostTabHelper(content::WebContents* contents)
    : content::WebContentsObserver(contents),
      content::WebContentsUserData<BoostTabHelper>(*contents) {
  prefs_.Init(PrefsOf(contents));
  const auto apply = base::BindRepeating(&BoostTabHelper::Apply,
                                         base::Unretained(this));
  prefs_.Add(kBoostsPref, apply);
  prefs_.Add(kBoostsEnabledPref, apply);
}

BoostTabHelper::~BoostTabHelper() = default;

void BoostTabHelper::DidFinishNavigation(content::NavigationHandle* handle) {
  if (!handle->IsInPrimaryMainFrame() || !handle->HasCommitted() ||
      handle->IsSameDocument()) {
    return;
  }
  if (zapping()) {
    FinishZap(std::string());
  }
  Apply();
}

void BoostTabHelper::DOMContentLoaded(content::RenderFrameHost* frame) {
  if (frame->IsInPrimaryMainFrame()) {
    Apply();
  }
}

void BoostTabHelper::DocumentOnLoadCompletedInPrimaryMainFrame() {
  // Pages that set their own adopted sheets after their start drop it.
  Apply();
}

void BoostTabHelper::Apply() {
  content::RenderFrameHost* frame = web_contents()->GetPrimaryMainFrame();
  if (!frame || !frame->IsRenderFrameLive()) {
    return;
  }
  PrefService* prefs = PrefsOf(web_contents());
  const std::string site = BoostSiteOf(frame->GetLastCommittedURL());
  if (site.empty()) {
    return;
  }
  const base::DictValue* boost = FindBoost(prefs, site);
  const std::string css = boost && prefs->GetBoolean(kBoostsEnabledPref)
                              ? BoostCss(*boost)
                              : std::string();
  const std::string script =
      std::string(kApplyScript) + "(" +
      base::WriteJson(base::Value(css)).value_or("\"\"") + ")";
  frame->ExecuteJavaScriptInIsolatedWorld(base::UTF8ToUTF16(script),
                                          base::NullCallback(),
                                          ISOLATED_WORLD_ID_CHROME_INTERNAL);
}

void BoostTabHelper::StartZap(
    base::OnceCallback<void(const std::string&)> done) {
  CancelZap();
  zap_done_ = std::move(done);
  web_contents()->GetPrimaryMainFrame()->ExecuteJavaScriptInIsolatedWorld(
      base::UTF8ToUTF16(std::string(kZapScript)), base::NullCallback(),
      ISOLATED_WORLD_ID_CHROME_INTERNAL);
  web_contents()->Focus();
  zap_poll_.Start(FROM_HERE, base::Milliseconds(120),
                  base::BindRepeating(&BoostTabHelper::PollZap,
                                      base::Unretained(this)));
}

void BoostTabHelper::CancelZap() {
  if (!zapping()) {
    return;
  }
  web_contents()->GetPrimaryMainFrame()->ExecuteJavaScriptInIsolatedWorld(
      base::UTF8ToUTF16(std::string(kZapCancelScript)), base::NullCallback(),
      ISOLATED_WORLD_ID_CHROME_INTERNAL);
  FinishZap(std::string());
}

void BoostTabHelper::PollZap() {
  web_contents()->GetPrimaryMainFrame()->ExecuteJavaScriptInIsolatedWorld(
      u"window.__alerseZapResult",
      base::BindOnce(
          [](base::WeakPtr<BoostTabHelper> self, base::Value result) {
            if (self && self->zapping() && result.is_string()) {
              self->FinishZap(result.GetString());
            }
          },
          weak_factory_.GetWeakPtr()),
      ISOLATED_WORLD_ID_CHROME_INTERNAL);
}

void BoostTabHelper::FinishZap(const std::string& selector) {
  zap_poll_.Stop();
  if (!selector.empty()) {
    PrefService* prefs = PrefsOf(web_contents());
    const std::string site =
        BoostSiteOf(web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL());
    const base::DictValue* old = FindBoost(prefs, site);
    base::DictValue boost = old ? old->Clone() : base::DictValue();
    base::ListValue* zaps = boost.EnsureList("zaps");
    zaps->Append(selector);
    SetBoost(prefs, site, std::move(boost));
  }
  if (zap_done_) {
    std::move(zap_done_).Run(selector);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(BoostTabHelper);

}  // namespace alerse
