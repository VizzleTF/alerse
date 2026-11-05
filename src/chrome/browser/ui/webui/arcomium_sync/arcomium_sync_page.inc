// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The files of chrome://arcomium-sync, served by ArcomiumSyncUI.

#ifndef CHROME_BROWSER_UI_WEBUI_ARCOMIUM_SYNC_ARCOMIUM_SYNC_PAGE_INC_
#define CHROME_BROWSER_UI_WEBUI_ARCOMIUM_SYNC_ARCOMIUM_SYNC_PAGE_INC_

inline constexpr char kArcomiumSyncHtml[] = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>Sync</title>
<link rel="stylesheet" href="base.css">
<link rel="stylesheet" href="arcomium_sync.css">
<script type="module" src="arcomium_sync.js"></script>
</head>
<body>
<main>
  <header>
    <h1>Sync</h1>
    <p class="lede">Keep bookmarks, tabs, passwords and settings the same on
      all your devices. Everything is encrypted with your sync code before it
      leaves this device: the server stores only data it cannot read.</p>
  </header>

  <section id="restart" class="banner" hidden>
    <p id="restart-text"></p>
    <button id="restart-button" class="primary">Restart Arcomium</button>
  </section>

  <section id="status" hidden>
    <div class="status-line">
      <span id="status-dot" class="dot"></span>
      <span id="status-text"></span>
    </div>
    <dl>
      <dt>Server</dt><dd id="status-server"></dd>
      <dt>Last synced</dt><dd id="status-synced"></dd>
    </dl>
    <div class="actions">
      <button id="show-code">Show sync code</button>
      <button id="leave" class="quiet">Leave sync chain</button>
    </div>
    <div id="leave-confirm" class="confirm" hidden>
      <p>This device stops syncing. Its bookmarks, tabs and passwords stay on
        it, and your other devices keep theirs.</p>
      <div class="actions">
        <button id="leave-yes" class="danger">Leave</button>
        <button id="leave-no">Cancel</button>
      </div>
    </div>
  </section>

  <section id="code" hidden>
    <h2>Your sync code</h2>
    <p>Type these 24 words on your other devices to add them. Keep the code
      somewhere safe: anyone who has it can read your synced data, and
      without it a new device cannot join.</p>
    <ol id="code-words" class="words"></ol>
    <div class="actions">
      <button id="copy-code">Copy</button>
      <span id="copy-done" class="note" hidden>Copied</span>
    </div>
  </section>

  <section id="setup" hidden>
    <form id="create" class="card" novalidate>
      <h2>Start syncing</h2>
      <p>For your first device. You get a sync code to add the others.</p>
      <label for="create-server">Sync server</label>
      <input id="create-server" type="url" inputmode="url" autocomplete="url"
          placeholder="https://sync.example.com" required>
      <p id="create-error" class="error" role="alert" hidden></p>
      <button class="primary" type="submit">Start</button>
    </form>

    <form id="join" class="card" novalidate>
      <h2>Join with a sync code</h2>
      <p>For a device you add to the ones that already sync.</p>
      <label for="join-server">Sync server</label>
      <input id="join-server" type="url" inputmode="url" autocomplete="url"
          placeholder="https://sync.example.com" required>
      <label for="join-code">Sync code</label>
      <textarea id="join-code" rows="3" spellcheck="false"
          autocapitalize="off" placeholder="24 words"></textarea>
      <p id="join-error" class="error" role="alert" hidden></p>
      <button class="primary" type="submit">Join</button>
    </form>
  </section>
</main>
</body>
</html>
)HTML";

inline constexpr char kArcomiumSyncCss[] = R"CSS(
.banner { border-color: var(--accent); }
.status-line { display: flex; align-items: center; gap: 8px; font-weight: 500; }
.dot { width: 8px; height: 8px; border-radius: 50%; background: var(--muted); flex: none; }
.dot.ok { background: var(--ok); }
.dot.warn { background: var(--warn); }
.dot.bad { background: var(--bad); }
dl { display: grid; grid-template-columns: max-content 1fr; gap: 4px 16px; margin: 0; }
dt { color: var(--muted); }
dd { margin: 0; overflow-wrap: anywhere; }
.confirm { display: grid; gap: 12px; border-top: 1px solid var(--line); padding-top: 12px; }
.words {
  list-style: none;
  counter-reset: word;
  margin: 0;
  padding: 0;
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(8.5rem, 1fr));
  gap: 6px;
  font: 14px/1.4 ui-monospace, "Cascadia Mono", Consolas, monospace;
  user-select: all;
}
.words li {
  counter-increment: word;
  background: var(--bg);
  border-radius: 6px;
  padding: 5px 8px;
}
.words li::before {
  content: counter(word) ".";
  display: inline-block;
  min-width: 2.2em;
  color: var(--muted);
  font-variant-numeric: tabular-nums;
}
)CSS";

inline constexpr char kArcomiumSyncJs[] = R"JS(
import {addWebUiListener, sendWithPromise} from 'chrome://resources/js/cr.js';

const $ = (id) => document.getElementById(id);

const STATUS = {
  active: ['ok', 'Syncing'],
  starting: ['warn', 'Connecting to the server…'],
  stopped: ['warn', 'Sync is paused'],
  off: ['', 'Not syncing'],
  wrongCode: ['bad', 'This device’s sync code does not open the chain’s ' +
      'data. Leave the sync chain and join again with the right code.'],
  refused: ['bad', 'The server refused this device. Check that the device’s ' +
      'clock is right, then restart Arcomium.'],
  error: ['bad', 'Sync stopped after an error. Restart Arcomium to try again.'],
};

function formatTime(ms) {
  if (!ms) return 'Not yet';
  const minutes = Math.round((Date.now() - ms) / 60000);
  if (minutes < 1) return 'Just now';
  if (minutes < 60) return `${minutes} min ago`;
  return new Date(ms).toLocaleString();
}

function showCode(code) {
  $('code-words').replaceChildren(...code.split(' ').map((word) => {
    const li = document.createElement('li');
    li.textContent = word;
    return li;
  }));
  $('code').dataset.code = code;
  $('code').hidden = false;
  $('copy-done').hidden = true;
}

function render(state) {
  const configured = state.server !== '';
  $('setup').hidden = configured;
  $('status').hidden = !configured || state.restartNeeded;
  if (!configured) {
    $('code').hidden = true;
  }

  $('restart').hidden = !state.restartNeeded;
  $('restart-text').textContent = configured ?
      'Restart Arcomium to start syncing.' :
      'This device left its sync chain. Restart Arcomium to finish.';

  const [tone, text] = STATUS[state.status] || STATUS.off;
  $('status-dot').className = 'dot ' + tone;
  $('status-text').textContent = text;
  $('status-server').textContent = state.server;
  $('status-synced').textContent = formatTime(state.lastSynced);
}

async function submit(form, errorId, send) {
  const button = form.querySelector('button[type=submit]');
  $(errorId).hidden = true;
  button.disabled = true;
  try {
    await send();
  } catch (message) {
    $(errorId).textContent = String(message);
    $(errorId).hidden = false;
  } finally {
    button.disabled = false;
  }
}

$('create').addEventListener('submit', (e) => {
  e.preventDefault();
  submit(e.target, 'create-error', async () => {
    const {code} = await sendWithPromise('createChain', $('create-server').value);
    showCode(code);
  });
});

$('join').addEventListener('submit', (e) => {
  e.preventDefault();
  submit(e.target, 'join-error', () =>
      sendWithPromise('joinChain', $('join-server').value, $('join-code').value));
});

$('show-code').addEventListener('click', async () => {
  try {
    const {code} = await sendWithPromise('showCode');
    showCode(code);
  } catch (message) {
    $('status-text').textContent = String(message);
  }
});

$('copy-code').addEventListener('click', async () => {
  try {
    await navigator.clipboard.writeText($('code').dataset.code);
    $('copy-done').hidden = false;
  } catch {
    getSelection().selectAllChildren($('code-words'));
  }
});

$('leave').addEventListener('click', () => { $('leave-confirm').hidden = false; });
$('leave-no').addEventListener('click', () => { $('leave-confirm').hidden = true; });
$('leave-yes').addEventListener('click', async () => {
  $('leave-confirm').hidden = true;
  $('code').hidden = true;
  await sendWithPromise('leaveChain');
});

$('restart-button').addEventListener('click', () => chrome.send('restart'));

addWebUiListener('state-changed', render);
sendWithPromise('getState').then(render);
)JS";

#endif  // CHROME_BROWSER_UI_WEBUI_ARCOMIUM_SYNC_ARCOMIUM_SYNC_PAGE_INC_
