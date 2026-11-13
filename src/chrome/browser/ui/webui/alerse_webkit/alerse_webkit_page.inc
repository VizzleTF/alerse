// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The files of chrome://alerse-webkit, served by AlerseWebKitUI. On macOS a
// bar sits above the WebKit view the page asks for; elsewhere the page says
// why the site's video cannot play and offers a browser that plays it.

#ifndef CHROME_BROWSER_UI_WEBUI_ALERSE_WEBKIT_ALERSE_WEBKIT_PAGE_INC_
#define CHROME_BROWSER_UI_WEBUI_ALERSE_WEBKIT_ALERSE_WEBKIT_PAGE_INC_

inline constexpr char kAlerseWebKitHtml[] = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>Protected video</title>
<link rel="stylesheet" href="base.css">
<link rel="stylesheet" href="alerse_webkit.css">
<script type="module" src="alerse_webkit.js"></script>
</head>
<body>
<header id="bar" class="bar" hidden>
  <button id="back" class="icon" type="button" title="Back"
      aria-label="Back" disabled>&#x2039;</button>
  <button id="reload" class="icon" type="button" title="Reload"
      aria-label="Reload">&#x21bb;</button>
  <span class="what"><strong id="bar-host"></strong>
    <span class="muted">plays in WebKit, which has Apple's DRM</span></span>
  <span class="grow"></span>
  <button id="bar-safari" class="link" type="button">Open in Safari</button>
  <button id="bar-chromium" class="link" type="button"
      title="Its protected video will not play">Open in Chromium</button>
</header>
<p id="loading" class="loading" hidden>Loading…</p>

<main id="explain" hidden>
  <h1><span id="host"></span> needs protected video</h1>
  <p class="lede">Its player uses Widevine, Google's DRM. Google licenses
    Widevine only to browsers it has signed, and Alerse is not one of them,
    so the video would not start here.</p>
  <div class="actions">
    <button id="elsewhere" class="primary" type="button" hidden>Open in
      Microsoft Edge</button>
    <button id="chromium" type="button">Open here anyway</button>
  </div>
  <p class="note">Edge plays it with Microsoft's own DRM. On a Mac, Alerse
    plays these sites in WebKit, Safari's engine.</p>
</main>
</body>
</html>
)HTML";

inline constexpr char kAlerseWebKitCss[] = R"CSS(
html, body { height: 100%; }
.bar {
  box-sizing: border-box;
  height: 40px;
  display: flex;
  align-items: center;
  gap: 6px;
  padding: 0 10px;
  border-bottom: 1px solid var(--line);
  background: var(--panel);
  font-size: 13px;
  white-space: nowrap;
  overflow: hidden;
}
.bar .what { overflow: hidden; text-overflow: ellipsis; }
.bar .grow { flex: 1; }
.muted { color: var(--muted); }
button.icon, button.link {
  border: 0;
  background: none;
  color: inherit;
  font: inherit;
  border-radius: 6px;
  cursor: pointer;
}
button.icon { width: 28px; height: 28px; font-size: 18px; line-height: 1; }
button.link { padding: 4px 8px; color: var(--accent); }
button.icon:hover:not(:disabled), button.link:hover { background: var(--hover); }
button.icon:disabled { opacity: .35; cursor: default; }
button:focus-visible { outline: 2px solid var(--ring); outline-offset: 1px; }
.loading { margin: 24px; color: var(--muted); }
.actions { display: flex; gap: 8px; flex-wrap: wrap; }
.actions button {
  padding: 8px 14px;
  border: 1px solid var(--line);
  border-radius: var(--radius);
  background: var(--panel);
  color: var(--fg);
  font: inherit;
  cursor: pointer;
}
.actions button.primary {
  border-color: var(--accent);
  background: var(--accent);
  color: var(--accent-fg);
}
.lede { color: var(--muted); }
.note { color: var(--muted); font-size: 13px; }
)CSS";

inline constexpr char kAlerseWebKitJs[] = R"JS(
import {addWebUiListener, sendWithPromise} from 'chrome://resources/js/cr.js';

const $ = (id) => document.getElementById(id);

// Keeps the tab's address the site's, so that it restores, syncs and is
// copied as the site.
function show(site, host, title) {
  if (title) {
    document.title = title;
  }
  $('bar-host').textContent = host;
  const page = '?u=' + encodeURIComponent(site);
  if (location.search !== page) {
    history.replaceState(null, '', page);
  }
}

const state = await sendWithPromise('getWebKitState');
$('host').textContent = state.host || 'This site';
document.title = state.host || 'Protected video';

if (state.platform === 'mac') {
  $('bar').hidden = false;
  $('loading').hidden = false;
  $('back').addEventListener('click', () => chrome.send('webKitBack'));
  $('reload').addEventListener('click', () => chrome.send('webKitReload'));
  $('bar-safari').addEventListener('click',
      () => chrome.send('openElsewhere'));
  $('bar-chromium').addEventListener('click',
      () => chrome.send('openInChromium'));
  addWebUiListener('webkit-state', (s) => {
    $('back').disabled = !s.canGoBack;
    $('loading').hidden = true;
    show(s.site, s.host, s.title);
  });
  show(state.site, state.host, '');
  chrome.send('startWebKit');
} else {
  $('explain').hidden = false;
  if (state.platform === 'win') {
    $('elsewhere').hidden = false;
    $('elsewhere').addEventListener('click',
        () => chrome.send('openElsewhere'));
  }
  $('chromium').addEventListener('click',
      () => chrome.send('openInChromium'));
}
)JS";

#endif  // CHROME_BROWSER_UI_WEBUI_ALERSE_WEBKIT_ALERSE_WEBKIT_PAGE_INC_
