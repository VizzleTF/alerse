// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The files of chrome://alerse-welcome, served by AlerseWelcomeUI. One step
// at a time; every step but the search engine can be skipped, and Back keeps
// what was picked. The import step uses the import page's messages.

#ifndef CHROME_BROWSER_UI_WEBUI_ALERSE_WELCOME_ALERSE_WELCOME_PAGE_INC_
#define CHROME_BROWSER_UI_WEBUI_ALERSE_WELCOME_ALERSE_WELCOME_PAGE_INC_

inline constexpr char kAlerseWelcomeHtml[] = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>Welcome to Alerse</title>
<link rel="stylesheet" href="base.css">
<link rel="stylesheet" href="alerse_welcome.css">
<script type="module" src="alerse_welcome.js"></script>
</head>
<body>
<main>
  <ol id="dots" class="dots" aria-label="Steps"></ol>

  <section class="step" data-step="intro">
    <h1>Welcome to Alerse</h1>
    <p class="lede">A few choices to make it yours. Each takes a moment, and
      every one stays in Settings.</p>
    <ul class="plain">
      <li>No account and no AI: nothing here signs you in to anything.</li>
      <li>Your Spaces, pinned tabs and folders can come with you from Arc.</li>
      <li>Sync is end-to-end encrypted, on your own server or Alerse's.</li>
    </ul>
  </section>

  <section class="step" data-step="source" hidden>
    <h1>Where are you coming from?</h1>
    <p class="lede">Alerse found these browsers on this computer. Import
      reads their files only, so they can stay open.</p>
    <p id="searching" class="note">Looking for browsers…</p>
    <p id="none" class="note" hidden>No other browser was found here. You can
      import later from the Alerse menu, Import from Another Browser.</p>
    <ul id="sources" class="sources"></ul>
  </section>

  <section class="step" data-step="search" hidden>
    <h1>Search engine</h1>
    <p class="lede">What the address bar and the command bar search with.
      Change it any time in Settings.</p>
    <div id="engines" class="choices" role="radiogroup"
         aria-label="Search engine"></div>
  </section>

  <section class="step" data-step="theme" hidden>
    <h1>Light or dark</h1>
    <p class="lede">For the browser's own surfaces; every Space can still have
      its own colors.</p>
    <div class="choices three" role="radiogroup" aria-label="Theme">
      <button class="choice" data-theme="system" role="radio">
        <span class="name">Match the system</span>
        <span class="note">Follows light and dark mode</span></button>
      <button class="choice" data-theme="light" role="radio">
        <span class="name">Light</span><span class="note">Always light</span></button>
      <button class="choice" data-theme="dark" role="radio">
        <span class="name">Dark</span><span class="note">Always dark</span></button>
    </div>
  </section>

  <section class="step" data-step="links" hidden>
    <h1>Looking at a link</h1>
    <p class="lede">Two ways to see a page without making a tab of it, as in
      Arc. Both stay on the Link routing page in the Alerse menu.</p>
    <div class="choices" role="group" aria-label="Looking at a link">
      <button class="choice" id="opt-peek" role="switch" aria-checked="true">
        <span class="name">Peek</span>
        <span class="note">Shift+click a link, or click a link to another site
          in a pinned tab: it opens over the page. Open as Tab keeps it.</span>
      </button>
      <button class="choice" id="opt-little" role="switch" aria-checked="true">
        <span class="name">Little window</span>
        <span class="note" id="little-note">A small window for a quick look,
          and where links from other apps open.</span>
      </button>
    </div>
  </section>

  <section class="step" data-step="sync" hidden>
    <h1>Sync, if you want it</h1>
    <p class="lede">Bookmarks, tabs, Spaces, passwords and settings, encrypted
      on this device before they leave it. A server of your own is the better
      choice; the Alerse server is there for those who would rather not run
      one, offered voluntarily and without any guarantee.</p>
    <div class="actions">
      <button id="sync-open">Set up sync</button>
      <span class="note">Opens the Sync page in a new tab. You can also do it
        later from the Alerse menu.</span>
    </div>
  </section>

  <section class="step" data-step="default" hidden>
    <h1>Make Alerse your browser</h1>
    <p class="lede">Links from other apps then open in Alerse.</p>
    <div class="actions">
      <button id="default-set">Make default</button>
      <span id="default-state" class="note"></span>
    </div>
  </section>

  <section class="step" data-step="done" hidden>
    <h1>You are set</h1>
    <ul id="summary" class="plain"></ul>
    <p id="tips" class="note">The sidebar is on the left. Ctrl+T opens the
      command bar, Ctrl+D pins the tab, and everything else is in the Alerse
      menu, the logo at the sidebar's top.</p>
  </section>

  <nav class="nav">
    <button id="back" class="quiet">Back</button>
    <span class="grow"></span>
    <button id="skip" class="quiet">Skip</button>
    <button id="next" class="primary">Continue</button>
  </nav>
</main>
</body>
</html>
)HTML";

inline constexpr char kAlerseWelcomeCss[] = R"CSS(
main { max-width: 640px; min-height: 100vh; display: grid;
  align-content: center; gap: 28px; }
.dots { display: flex; gap: 8px; list-style: none; margin: 0; padding: 0; }
.dots li { width: 8px; height: 8px; border-radius: 50%; background: var(--line); }
.dots li.on { background: var(--accent); }
.step { display: grid; gap: 16px; }
@media (prefers-reduced-motion: no-preference) {
  .step:not([hidden]) { animation: step-in 240ms ease-out; }
  .choice { transition: border-color 150ms, box-shadow 150ms, background 150ms; }
  .dots li { transition: background 200ms; }
}
@keyframes step-in { from { opacity: 0; transform: translateY(6px); } }
.plain { margin: 0; padding: 0; list-style: none; display: grid; gap: 8px; }
.plain li::before { content: "— "; color: var(--muted); }
.choices { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
.choices.three { grid-template-columns: repeat(3, 1fr); }
.choice { display: grid; gap: 2px; width: 100%; text-align: left;
  align-content: start; padding: 12px 14px; border-radius: var(--radius-lg); }
.choice .name { font-weight: 500; }
.choice .note { font-size: 13px; }
.choice[aria-checked="true"] { border-color: var(--accent);
  box-shadow: inset 0 0 0 1px var(--accent); }
.sources { margin: 0; padding: 0; list-style: none; display: grid; gap: 10px; }
.source { display: flex; align-items: center; gap: 12px; padding: 12px 14px;
  border: 1px solid var(--line); border-radius: var(--radius-lg);
  background: var(--panel); }
.source .what { flex: 1; display: grid; gap: 2px; }
.source .note.result { color: var(--ok); }
.source .note.error { color: var(--bad); }
.source .extensions { display: flex; align-items: center; gap: 8px;
  margin-top: 4px; }
.source .extensions .note { flex: 1; }
.actions { display: flex; align-items: center; gap: 12px; flex-wrap: wrap; }
.nav { display: flex; align-items: center; gap: 10px; }
.grow { flex: 1; }
@media (max-width: 560px) {
  .choices, .choices.three { grid-template-columns: 1fr; }
}
)CSS";

inline constexpr char kAlerseWelcomeJs[] = R"JS(
import {sendWithPromise} from 'chrome://resources/js/cr.js';

const $ = (id) => document.getElementById(id);
const steps = [...document.querySelectorAll('.step')];
// The search engine has no Skip: a first start picks one, and Google is
// one of the choices rather than the default.
const noSkip = new Set(['intro', 'search', 'done']);
let index = 0;
const picked = {};

for (const _ of steps) {
  $('dots').append(document.createElement('li'));
}

function show(i) {
  index = Math.max(0, Math.min(steps.length - 1, i));
  steps.forEach((s, n) => { s.hidden = n !== index; });
  [...$('dots').children].forEach((d, n) => d.classList.toggle('on', n <= index));
  const name = steps[index].dataset.step;
  $('back').hidden = index === 0;
  $('skip').hidden = noSkip.has(name);
  $('next').textContent = name === 'done' ? 'Start browsing' :
      name === 'intro' ? 'Begin' : 'Continue';
  if (name === 'source') findSources();
  if (name === 'default') checkDefault(false);
  if (name === 'done') summarize();
  steps[index].querySelector('h1').focus?.();
}

function radio(container, button) {
  container.querySelectorAll('.choice').forEach(
      (b) => b.setAttribute('aria-checked', String(b === button)));
}

// Where you come from: the import page's sources and import.
let searched = false;
async function findSources() {
  if (searched) return;
  searched = true;
  let sources = [];
  try {
    sources = await sendWithPromise('getSources');
  } catch (e) {}
  $('searching').hidden = true;
  $('none').hidden = sources.length > 0;
  for (const source of sources) {
    const li = document.createElement('li');
    li.className = 'source';
    const what = document.createElement('div');
    what.className = 'what';
    const name = document.createElement('strong');
    name.textContent = source.browser + (source.profile ? ' — ' + source.profile : '');
    const note = document.createElement('span');
    note.className = 'note';
    note.textContent = source.kind === 'arc' ?
        `${source.spaces} Spaces, ${source.links} pinned links` :
        `${source.links} bookmarks`;
    what.append(name, note);
    const button = document.createElement('button');
    button.textContent = 'Import';
    button.addEventListener('click', async () => {
      button.disabled = true;
      button.textContent = 'Importing…';
      try {
        const r = await sendWithPromise('importSource', source.id);
        note.className = 'note result';
        note.textContent = source.kind === 'arc' ?
            `Imported ${r.spaces} Spaces and ${r.links} links` :
            `Imported ${r.links} bookmarks into the folder ${r.folder}`;
        button.textContent = 'Imported';
        picked.imported = (picked.imported || []).concat(source.browser);
      } catch (code) {
        note.className = 'note error';
        note.textContent = code === 'busy' ? 'Another import is running.' :
            'This browser could not be read. Close it and try again.';
        button.disabled = false;
        button.textContent = 'Import';
      }
    });
    li.append(what, button);
    $('sources').append(li);
    offerExtensions(source, what, EXTENSION_TEXT);
  }
}

const EXTENSION_TEXT = {
  summary: (n, names) => `${n} extension${n === 1 ? '' : 's'} from the ` +
      `Chrome Web Store: ${names}`,
  add: (n) => n === 1 ? 'Add it' : `Add ${n}`,
  working: 'Adding…',
  done: (added, failed) => `Added ${added}` +
      (failed ? `; ${failed} could not be added` : ''),
  failed: 'The extensions could not be read. Close the browser and try again.',
};

// The source's Chrome Web Store extensions that this Space does not have,
// with a button that adds them from the source's copies.
async function offerExtensions(source, parent, text) {
  let found = [];
  try {
    found = await sendWithPromise('getExtensions', source.id);
  } catch (e) {
    return;
  }
  const missing = found.filter((e) => !e.installed);
  if (missing.length === 0) return;
  const row = document.createElement('div');
  row.className = 'extensions';
  const note = document.createElement('p');
  note.className = 'note';
  const names = missing.slice(0, 4).map((e) => e.name).join(', ') +
      (missing.length > 4 ? ', …' : '');
  note.textContent = text.summary(missing.length, names);
  const button = document.createElement('button');
  button.type = 'button';
  button.textContent = text.add(missing.length);
  button.addEventListener('click', async () => {
    button.disabled = true;
    button.textContent = text.working;
    try {
      const r = await sendWithPromise('importExtensions', source.id);
      note.className = r.failed ? 'note error' : 'note result';
      note.textContent = text.done(r.added, r.failed);
      button.remove();
    } catch (code) {
      note.className = 'note error';
      note.textContent = text.failed;
      button.disabled = false;
      button.textContent = text.add(missing.length);
    }
  });
  row.append(note, button);
  parent.append(row);
}

async function loadState() {
  const state = await sendWithPromise('getWelcomeState');
  for (const engine of state.engines) {
    const b = document.createElement('button');
    b.className = 'choice';
    b.setAttribute('role', 'radio');
    b.dataset.id = engine.id;
    // Built node by node: chrome:// pages enforce Trusted Types.
    const name = document.createElement('span');
    name.className = 'name';
    name.textContent = engine.name;
    const note = document.createElement('span');
    note.className = 'note';
    note.textContent = engine.note;
    b.append(name, note);
    b.addEventListener('click', () => {
      radio($('engines'), b);
      picked.engine = engine;
    });
    $('engines').append(b);
  }
  // The first offered engine, a private one where the region's list has
  // one, stands chosen until another is picked: the first start does not keep
  // Google just because it was there.
  const first = $('engines').querySelector('.choice');
  if (first) {
    radio($('engines'), first);
    picked.engine = state.engines[0];
  }
  const theme = document.querySelector(`[data-theme="${state.theme}"]`);
  if (theme) radio(theme.parentElement, theme);
  picked.theme = state.theme;
  $('opt-peek').setAttribute('aria-checked', String(state.options.peek));
  $('opt-little').setAttribute('aria-checked',
                               String(state.options.littleWindow));
}

for (const [id, name] of [['opt-peek', 'peek'], ['opt-little', 'littleWindow']]) {
  $(id).addEventListener('click', async () => {
    const on = $(id).getAttribute('aria-checked') !== 'true';
    $(id).setAttribute('aria-checked', String(on));
    await sendWithPromise('setLinkOption', name, on);
  });
}
if (navigator.platform.startsWith('Mac')) {
  $('little-note').textContent = 'Option-Command-N opens a small window ' +
      'for a quick look, and links from other apps open in one.';
} else {
  $('little-note').textContent = 'Ctrl+Alt+N opens a small window for a ' +
      'quick look, and links from other apps open in one.';
}

document.querySelectorAll('[data-theme]').forEach((b) => {
  b.addEventListener('click', async () => {
    radio(b.parentElement, b);
    picked.theme = b.dataset.theme;
    await sendWithPromise('setTheme', picked.theme);
  });
});

$('sync-open').addEventListener('click', () => {
  window.open('chrome://alerse-sync/', '_blank');
  picked.sync = true;
});

async function checkDefault(set) {
  const answer = await sendWithPromise('setDefaultBrowser', set);
  $('default-state').textContent = answer === 'yes' ?
      'Alerse is your default browser.' :
      set ? 'Finish it in the settings the system opened.' : '';
  $('default-set').hidden = answer === 'yes';
  picked.isDefault = answer === 'yes';
}
$('default-set').addEventListener('click', () => checkDefault(true));

function summarize() {
  const lines = [];
  if (picked.imported) lines.push('Imported from ' + picked.imported.join(', '));
  if (picked.engine) lines.push('Searching with ' + picked.engine.name);
  if (picked.theme) lines.push({system: 'Theme follows the system', light: 'Light theme', dark: 'Dark theme'}[picked.theme]);
  if (picked.sync) lines.push('Sync set up in its own tab');
  if (picked.isDefault) lines.push('Alerse is your default browser');
  $('summary').replaceChildren(...lines.map((t) => {
    const li = document.createElement('li');
    li.textContent = t;
    return li;
  }));
}

$('back').addEventListener('click', () => show(index - 1));
$('skip').addEventListener('click', () => show(index + 1));
$('next').addEventListener('click', async () => {
  const name = steps[index].dataset.step;
  if (name === 'search' && picked.engine) {
    await sendWithPromise('setSearchEngine', picked.engine.id);
  }
  if (name === 'done') {
    location.href = 'chrome://newtab/';
    return;
  }
  show(index + 1);
});

// The shortcuts in the platform's own keys.
if (navigator.userAgent.includes('Mac OS')) {
  $('tips').textContent = $('tips').textContent.replaceAll('Ctrl+', '⌘');
}

loadState().then(() => show(0));
)JS";

#endif  // CHROME_BROWSER_UI_WEBUI_ALERSE_WELCOME_ALERSE_WELCOME_PAGE_INC_
