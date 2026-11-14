// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The files of chrome://alerse-boost, served by AlerseBoostUI.

#ifndef CHROME_BROWSER_UI_WEBUI_ALERSE_BOOST_ALERSE_BOOST_PAGE_INC_
#define CHROME_BROWSER_UI_WEBUI_ALERSE_BOOST_ALERSE_BOOST_PAGE_INC_

inline constexpr char kAlerseBoostHtml[] = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>Boosts</title>
<link rel="stylesheet" href="base.css">
<link rel="stylesheet" href="alerse_boost.css">
<script type="module" src="alerse_boost.js"></script>
</head>
<body>
<div id="panel" hidden>
  <header class="bar">
    <div class="title">
      <strong>Boost</strong>
      <span id="site" class="site"></span>
    </div>
    <button id="site-on" class="toggle" role="switch" aria-checked="false"
        title="This site's boost on or off"></button>
    <button id="close" class="icon" title="Close (Esc)"
        aria-label="Close">&#x2715;</button>
  </header>

  <p id="all-off" class="banner" hidden>Boosts are turned off.
    <button id="turn-on" class="quiet">Turn on</button></p>

  <section class="group">
    <div class="label-row"><h2>Color</h2>
      <button id="hue-reset" class="quiet small">None</button></div>
    <input id="hue" type="range" min="0" max="359" step="1"
        aria-label="Color">
    <label class="row"><input id="dark" type="checkbox">
      <span>Dark</span><span class="note">Light pages turn dark, pictures
        keep their colors</span></label>
  </section>

  <section class="group">
    <h2>Font</h2>
    <div class="fonts" role="radiogroup" aria-label="Font">
      <button class="font" data-font="" role="radio">Site's</button>
      <button class="font" data-font="sans" role="radio"
          style="font-family:system-ui,sans-serif">Sans</button>
      <button class="font" data-font="serif" role="radio"
          style="font-family:Charter,Georgia,serif">Serif</button>
      <button class="font" data-font="mono" role="radio"
          style="font-family:ui-monospace,Menlo,Consolas,monospace">Mono</button>
      <button class="font" data-font="rounded" role="radio"
          style="font-family:ui-rounded,'Arial Rounded MT Bold',sans-serif">Round</button>
    </div>
    <div class="label-row"><span>Size</span>
      <output id="size-out">100%</output></div>
    <input id="size" type="range" min="60" max="160" step="5" value="100"
        aria-label="Size">
  </section>

  <section class="group">
    <div class="label-row"><h2>Zap</h2>
      <button id="zap" class="primary small">Zap an element</button></div>
    <p class="note">Click a part of the page to hide it on this site. Esc
      stops.</p>
    <ul id="zaps" class="zaps"></ul>
  </section>

  <section class="group">
    <details id="code-box">
      <summary>Code</summary>
      <textarea id="css" rows="6" spellcheck="false"
          placeholder="/* CSS for this site */"></textarea>
    </details>
  </section>

  <footer class="bar foot">
    <button id="open-list" class="quiet small">All Boosts</button>
    <button id="delete" class="danger small">Delete Boost</button>
  </footer>
</div>

<main id="list" hidden>
  <header>
    <h1>Boosts</h1>
    <p class="lede">Your own look for a site: its colors, font and size,
      parts of it zapped away, your CSS. Only you see them. To boost a site,
      open the site controls in the address and choose Boost, or run
      Boost This Site in the command bar.</p>
  </header>
  <section class="card">
    <label class="switch-row"><input id="all" type="checkbox">
      <span><strong>Boosts</strong>
        <span class="note">Turned off, every site looks as it is.</span>
      </span></label>
  </section>
  <section class="card">
    <h2>Sites</h2>
    <p id="none" class="note">No boosts yet.</p>
    <ul id="sites" class="sites"></ul>
  </section>
</main>
</body>
</html>
)HTML";

inline constexpr char kAlerseBoostCss[] = R"CSS(
body.panel { margin: 0; padding: 0; background: var(--panel);
  font-size: 13px; overflow-x: hidden; }
#panel { display: flex; flex-direction: column; gap: 4px; padding: 0 14px 12px; }
.bar { display: flex; align-items: center; gap: 8px; }
header.bar { position: sticky; top: 0; background: var(--panel);
  padding: 12px 0 8px; z-index: 1; }
.title { flex: 1; min-width: 0; display: flex; flex-direction: column; }
.title strong { font-size: 15px; }
.site { color: var(--muted); overflow: hidden; text-overflow: ellipsis;
  white-space: nowrap; }
button.icon { width: 28px; height: 28px; padding: 0; border-radius: 50%; }
button.small { padding: 4px 10px; font-size: 12px; }
.toggle { width: 38px; height: 22px; padding: 0; border-radius: 11px;
  position: relative; background: var(--line); border: none; }
.toggle::after { content: ''; position: absolute; top: 3px; left: 3px;
  width: 16px; height: 16px; border-radius: 50%; background: #fff;
  box-shadow: 0 1px 2px rgb(0 0 0 / .3); transition: transform 90ms ease-out; }
.toggle[aria-checked="true"] { background: var(--accent); }
.toggle[aria-checked="true"]::after { transform: translateX(16px); }
.toggle:hover:not(:disabled) { filter: brightness(1.05); }
.group { border-top: 1px solid var(--line); padding: 10px 0 6px;
  margin: 0; display: flex; flex-direction: column; gap: 8px; }
.group h2 { font-size: 13px; margin: 0; }
.label-row { display: flex; align-items: center; justify-content: space-between; }
.row { display: grid; grid-template-columns: auto auto 1fr; gap: 8px;
  align-items: center; }
.row .note { margin: 0; }
input[type="range"] { width: 100%; margin: 0; padding: 0; border: none;
  box-sizing: border-box; background: none; }
#hue { appearance: none; height: 14px; border-radius: 7px; border: none;
  padding: 0; background: linear-gradient(90deg, #ff4d4d, #ffb34d, #f7e34d,
  #6be36b, #4dd9e3, #4d7cff, #a64dff, #ff4dd2, #ff4d4d); }
#hue.none { opacity: .45; }
#hue::-webkit-slider-thumb { appearance: none; width: 18px; height: 18px;
  border-radius: 50%; background: #fff; border: 2px solid rgb(0 0 0 / .35);
  box-shadow: 0 1px 3px rgb(0 0 0 / .3); }
.fonts { display: grid; grid-template-columns: repeat(5, 1fr); gap: 6px; }
.font { padding: 8px 0; font-size: 13px; }
.font[aria-checked="true"] { outline: 2px solid var(--accent);
  outline-offset: -2px; }
.zaps, .sites { list-style: none; margin: 0; padding: 0; display: flex;
  flex-direction: column; gap: 4px; }
.zaps li, .sites li { display: flex; align-items: center; gap: 8px; }
.zaps code { flex: 1; min-width: 0; overflow: hidden; text-overflow: ellipsis;
  white-space: nowrap; font-size: 11px; color: var(--muted); }
textarea#css { font: 12px ui-monospace, Menlo, Consolas, monospace;
  width: 100%; box-sizing: border-box; margin-top: 6px; }
summary { cursor: pointer; font-weight: 600; }
.foot { justify-content: space-between; border-top: 1px solid var(--line);
  padding-top: 10px; }
.banner { margin: 4px 0; display: flex; align-items: center;
  justify-content: space-between; }
.switch-row { display: grid; grid-template-columns: auto 1fr; gap: 10px;
  align-items: start; cursor: pointer; }
.switch-row input { margin-top: 3px; }
.switch-row .note { display: block; }
.sites li { padding: 6px 0; border-bottom: 1px solid var(--line); }
.sites li:last-child { border-bottom: none; }
.sites .name { flex: 1; font-weight: 600; }
.sites .what { color: var(--muted); font-size: 12px; }
)CSS";

inline constexpr char kAlerseBoostJs[] = R"JS(
import {sendWithPromise} from 'chrome://resources/js/cr.js';

const $ = id => document.getElementById(id);
const inPanel = new URLSearchParams(location.search).has('tab');
let state = null;
let boost = {};

function describe(b) {
  const parts = [];
  if (b.hue >= 0) parts.push('color');
  if (b.dark) parts.push('dark');
  if (b.font) parts.push('font');
  if (b.size && b.size !== 100) parts.push(b.size + '%');
  if (b.zaps && b.zaps.length) parts.push(b.zaps.length + ' zapped');
  if (b.css && b.css.trim()) parts.push('code');
  return parts.join(', ') || 'nothing yet';
}

function renderPanel() {
  $('site').textContent = state.site || 'This page cannot be boosted';
  const on = boost.enabled !== false;
  $('site-on').setAttribute('aria-checked', String(on && !!state.site));
  $('all-off').hidden = state.enabled;
  const hue = boost.hue ?? -1;
  $('hue').value = hue >= 0 ? hue : 0;
  $('hue').classList.toggle('none', hue < 0);
  $('dark').checked = !!boost.dark;
  for (const b of document.querySelectorAll('.font')) {
    b.setAttribute('aria-checked', String((boost.font || '') === b.dataset.font));
  }
  $('size').value = boost.size || 100;
  $('size-out').textContent = (boost.size || 100) + '%';
  const list = $('zaps');
  list.replaceChildren();
  (boost.zaps || []).forEach((selector, i) => {
    const li = document.createElement('li');
    const code = document.createElement('code');
    code.textContent = selector;
    code.title = selector;
    const undo = document.createElement('button');
    undo.className = 'quiet small';
    undo.textContent = 'Restore';
    undo.addEventListener('click', () => {
      const zaps = [...boost.zaps];
      zaps.splice(i, 1);
      update({zaps});
    });
    li.append(code, undo);
    list.append(li);
  });
  if (document.activeElement !== $('css')) $('css').value = boost.css || '';
  if (boost.css) $('code-box').open = true;
  for (const el of document.querySelectorAll('#panel button, #panel input, #panel textarea')) {
    if (el.id !== 'close' && el.id !== 'open-list') el.disabled = !state.site;
  }
  $('delete').disabled = !state.site || !Object.keys(state.boost).length;
}

function renderList() {
  $('all').checked = state.enabled;
  const list = $('sites');
  list.replaceChildren();
  $('none').hidden = state.boosts.length > 0;
  for (const {site, boost: b} of state.boosts) {
    const li = document.createElement('li');
    const box = document.createElement('input');
    box.type = 'checkbox';
    box.checked = b.enabled !== false;
    box.title = 'On or off';
    box.addEventListener('change', async () => {
      apply(await sendWithPromise('setBoost', site, {...b, enabled: box.checked}));
    });
    const text = document.createElement('div');
    text.className = 'name';
    text.textContent = site;
    const what = document.createElement('div');
    what.className = 'what';
    what.textContent = describe(b);
    text.append(what);
    const remove = document.createElement('button');
    remove.className = 'danger small';
    remove.textContent = 'Delete';
    remove.addEventListener('click', async () => {
      apply(await sendWithPromise('setBoost', site, {}));
    });
    li.append(box, text, remove);
    list.append(li);
  }
}

function apply(s) {
  state = s;
  boost = {...s.boost};
  if (inPanel) renderPanel(); else renderList();
}

let pending = Promise.resolve();
function update(change) {
  boost = {...boost, ...change};
  for (const k of Object.keys(boost)) {
    if (boost[k] === undefined) delete boost[k];
  }
  const site = state.site;
  const send = {...boost};
  pending = pending.then(async () => {
    const s = await sendWithPromise('setBoost', site, send);
    state = s;
    if (inPanel && document.activeElement?.type !== 'range') renderPanel();
  });
}

if (inPanel) {
  document.body.classList.add('panel');
  $('panel').hidden = false;
  $('close').addEventListener('click', () => chrome.send('close'));
  addEventListener('keydown', e => {
    if (e.key === 'Escape') chrome.send('close');
  });
  $('site-on').addEventListener('click', () => {
    const on = $('site-on').getAttribute('aria-checked') !== 'true';
    $('site-on').setAttribute('aria-checked', String(on));
    update({enabled: on});
  });
  $('turn-on').addEventListener('click', async () => {
    apply(await sendWithPromise('setEnabled', true));
  });
  $('hue').addEventListener('input', () => {
    $('hue').classList.remove('none');
    update({hue: Math.max(1, Number($('hue').value)), enabled: true});
  });
  $('hue-reset').addEventListener('click', () => {
    update({hue: undefined});
    renderPanel();
  });
  $('dark').addEventListener('change', () => update({dark: $('dark').checked || undefined, enabled: true}));
  for (const b of document.querySelectorAll('.font')) {
    b.addEventListener('click', () => {
      update({font: b.dataset.font || undefined, enabled: true});
      renderPanel();
    });
  }
  $('size').addEventListener('input', () => {
    const size = Number($('size').value);
    $('size-out').textContent = size + '%';
    update({size: size === 100 ? undefined : size, enabled: true});
  });
  $('zap').addEventListener('click', async () => {
    await pending;
    apply(await sendWithPromise('zap'));
  });
  let typing = 0;
  $('css').addEventListener('input', () => {
    clearTimeout(typing);
    typing = setTimeout(() => update({css: $('css').value || undefined, enabled: true}), 250);
  });
  $('delete').addEventListener('click', async () => {
    apply(await sendWithPromise('setBoost', state.site, {}));
  });
  $('open-list').addEventListener('click', () => chrome.send('openList'));
} else {
  $('list').hidden = false;
  $('all').addEventListener('change', async () => {
    apply(await sendWithPromise('setEnabled', $('all').checked));
  });
}

sendWithPromise('getState').then(apply);
)JS";

#endif  // CHROME_BROWSER_UI_WEBUI_ALERSE_BOOST_ALERSE_BOOST_PAGE_INC_
