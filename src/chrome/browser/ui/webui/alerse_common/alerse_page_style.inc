// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The look shared by Alerse's own pages (chrome://alerse-sync,
// chrome://alerse-links), served to each as base.css.

#ifndef CHROME_BROWSER_UI_WEBUI_ALERSE_COMMON_ALERSE_PAGE_STYLE_INC_
#define CHROME_BROWSER_UI_WEBUI_ALERSE_COMMON_ALERSE_PAGE_STYLE_INC_

inline constexpr char kAlersePageBaseCss[] = R"CSS(
:root {
  --bg: #f6f5f8;
  --panel: #ffffff;
  --fg: #1d1b22;
  --muted: #5f5b69;
  --line: #e2dfe8;
  --accent: #5b4bdb;
  --accent-fg: #ffffff;
  --ok: #1f8a4c;
  --warn: #b26a00;
  --bad: #c62f3a;
  color-scheme: light dark;
}
@media (prefers-color-scheme: dark) {
  :root {
    --bg: #17161b;
    --panel: #211f27;
    --fg: #ecebf0;
    --muted: #a7a3b2;
    --line: #34313d;
    --accent: #8d80f2;
    --accent-fg: #17161b;
    --ok: #4cc27f;
    --warn: #e2a443;
    --bad: #f0707a;
  }
}
body {
  margin: 0;
  background: var(--bg);
  color: var(--fg);
  font: 14px/1.5 system-ui, "Segoe UI", sans-serif;
}
main {
  max-width: 36rem;
  margin: 0 auto;
  padding: 48px 20px;
  display: grid;
  gap: 24px;
}
h1 { margin: 0 0 8px; font-size: 28px; font-weight: 600; }
h2 { margin: 0 0 4px; font-size: 16px; font-weight: 600; }
p { margin: 0; }
.lede, .card > p, #code > p { color: var(--muted); }
section, .card { display: grid; gap: 12px; }
/* Author display rules such as the one above beat the user agent's
   [hidden] rule, so a hidden section would still show. */
[hidden] { display: none !important; }
.card, #status, #code, .banner {
  background: var(--panel);
  border: 1px solid var(--line);
  border-radius: 12px;
  padding: 20px;
}
#setup { gap: 16px; }
label { font-weight: 500; margin-bottom: -6px; }
input, textarea {
  font: inherit;
  color: inherit;
  background: var(--bg);
  border: 1px solid var(--line);
  border-radius: 8px;
  padding: 8px 10px;
}
textarea { resize: vertical; }
input:focus-visible, textarea:focus-visible, button:focus-visible {
  outline: 2px solid var(--accent);
  outline-offset: 1px;
}
button {
  font: inherit;
  font-weight: 500;
  color: var(--fg);
  background: transparent;
  border: 1px solid var(--line);
  border-radius: 8px;
  padding: 7px 14px;
  cursor: pointer;
  justify-self: start;
}
button.primary {
  background: var(--accent);
  border-color: var(--accent);
  color: var(--accent-fg);
}
button.danger { color: var(--bad); border-color: var(--bad); }
button.quiet { color: var(--muted); }
button:disabled { opacity: .6; cursor: default; }
.actions { display: flex; gap: 8px; align-items: center; flex-wrap: wrap; }
.error { color: var(--bad); }
.note { color: var(--muted); }
)CSS";

#endif  // CHROME_BROWSER_UI_WEBUI_ALERSE_COMMON_ALERSE_PAGE_STYLE_INC_
