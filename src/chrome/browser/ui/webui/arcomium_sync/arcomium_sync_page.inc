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
      <button id="pair-start">Add a device</button>
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

  <section id="pair" hidden>
    <h2>Add a device</h2>
    <div id="pair-show" class="sub">
      <p class="note">On the new device, open Sync, choose “Pair with another
        device” and type this code. It works once, for 5 minutes.</p>
      <div id="pair-code" class="pair-code"></div>
      <canvas id="pair-qr" width="200" height="200"
          aria-label="QR code of the pairing"></canvas>
      <p class="note">The QR code is for phones. <span id="pair-expires"></span></p>
    </div>
    <div id="pair-ask" class="sub" hidden>
      <p>Allow <strong id="pair-name"></strong> to join this sync chain? It
        gets your bookmarks, tabs, passwords and settings. Check that it
        shows the same number:</p>
      <div id="pair-number" class="pair-number"></div>
      <div class="actions">
        <button id="pair-allow" class="primary">Allow</button>
        <button id="pair-deny">Deny</button>
      </div>
    </div>
    <p id="pair-result" class="note" hidden></p>
    <p id="pair-error" class="error" role="alert" hidden></p>
    <div class="actions">
      <button id="pair-close" class="quiet">Close</button>
    </div>
  </section>

  <section id="login" hidden>
    <h2>Email and password</h2>
    <p id="login-text" class="note"></p>
    <div id="login-actions" class="actions">
      <button id="login-set">Set up</button>
      <button id="login-remove" class="quiet" hidden>Remove</button>
    </div>
    <form id="login-form" class="sub" novalidate hidden>
      <div class="login-fields" data-prefix="login"></div>
      <p id="login-error" class="error" role="alert" hidden></p>
      <div class="actions">
        <button class="primary" type="submit">Save</button>
        <button id="login-cancel" type="button">Cancel</button>
      </div>
    </form>
    <div id="login-remove-confirm" class="confirm" hidden>
      <p>New devices can then join only with the 24 words. Devices that
        already sync are not affected.</p>
      <p id="login-remove-error" class="error" role="alert" hidden></p>
      <div class="actions">
        <button id="login-remove-yes" class="danger">Remove</button>
        <button id="login-remove-no">Cancel</button>
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
      <button id="recovery-sheet">Print recovery sheet</button>
      <span id="copy-done" class="note" hidden>Copied</span>
    </div>
    <p class="note">The recovery sheet has the code, the server and how to
      add a device. Print it, or choose Save as PDF in the print dialog, and
      keep it somewhere safe. It is made on this device; nothing is sent
      anywhere.</p>
  </section>

  <section id="setup" hidden>
    <form id="create" class="card" novalidate>
      <h2>Start syncing</h2>
      <p>For your first device. You get a sync code to add the others.</p>
      <label for="create-server">Sync server</label>
      <input id="create-server" type="url" inputmode="url" autocomplete="url"
          placeholder="https://sync.example.com" required>
      <details id="create-login-box">
        <summary>Add an email and password (optional)</summary>
        <div class="sub">
          <p class="note">Your other devices can then join with them instead
            of typing the 24 words. The words keep working.</p>
          <div class="login-fields" data-prefix="create"></div>
        </div>
      </details>
      <p id="create-error" class="error" role="alert" hidden></p>
      <button class="primary" type="submit">Start</button>
    </form>

    <form id="join" class="card" novalidate>
      <h2>Join a sync chain</h2>
      <p>For a device you add to the ones that already sync.</p>
      <label for="join-server">Sync server</label>
      <input id="join-server" type="url" inputmode="url" autocomplete="url"
          placeholder="https://sync.example.com" required>
      <div class="segmented" role="radiogroup" aria-label="Join with">
        <label><input type="radio" name="join-with" value="code" checked>
          Sync code (24 words)</label>
        <label><input type="radio" name="join-with" value="login">
          Email and password</label>
        <label><input type="radio" name="join-with" value="pair">
          Pair with another device</label>
      </div>
      <div id="join-with-code" class="group">
        <label for="join-code">Sync code</label>
        <textarea id="join-code" rows="3" spellcheck="false"
            autocapitalize="off" placeholder="24 words"></textarea>
      </div>
      <div id="join-with-pair" class="group" hidden>
        <p class="note">On a device that already syncs, open Sync and choose
          Add a device. Type the code it shows.</p>
        <label for="join-pair-code">Pairing code</label>
        <input id="join-pair-code" type="text" spellcheck="false"
            autocapitalize="characters" autocomplete="off"
            placeholder="XXXX-XXXX-XXXX">
        <div id="join-pair-wait" class="sub" hidden>
          <p>On the other device, allow this one to join. Check that it shows
            this number:</p>
          <div id="join-pair-number" class="pair-number"></div>
        </div>
      </div>
      <div id="join-with-login" class="group" hidden>
        <label for="join-login">Email or login name</label>
        <input id="join-login" type="text" autocomplete="username"
            spellcheck="false" autocapitalize="off">
        <label for="join-password">Password</label>
        <input id="join-password" type="password"
            autocomplete="current-password">
      </div>
      <p id="join-error" class="error" role="alert" hidden></p>
      <button class="primary" type="submit">Join</button>
    </form>
  </section>
</main>
<section id="sheet" aria-hidden="true"></section>
<template id="login-fields">
  <label data-for="login">Email or login name</label>
  <input data-field="login" type="text" autocomplete="username"
      spellcheck="false" autocapitalize="off">
  <label data-for="password">Password</label>
  <input data-field="password" type="password" autocomplete="new-password">
  <div class="meter" aria-hidden="true"><span data-field="meter"></span></div>
  <p class="note" data-field="hint"></p>
  <label data-for="confirm">Confirm password</label>
  <input data-field="confirm" type="password" autocomplete="new-password">
  <p class="note">Whoever runs the server could try to guess the password
    offline, so make it long. Nobody checks that the email is yours. If you
    forget the password, the 24 words still work.</p>
</template>
</body>
</html>
)HTML";

inline constexpr char kArcomiumSyncCss[] = R"CSS(
/* The page's own display rules would otherwise show hidden blocks. */
[hidden] { display: none !important; }
#sheet { display: none; }
@media print {
  body { background: #fff; color: #111; }
  main { display: none; }
  #sheet { display: block; max-width: 44rem; margin: 0 auto; }
  #sheet .note { color: #555; }
  #sheet table { border-collapse: collapse; width: 100%; margin: 16px 0; }
  #sheet th, #sheet td { text-align: left; padding: 6px 8px;
    border-bottom: 1px solid #ccc; }
  #sheet th { width: 12rem; }
  #sheet td.blank { height: 28px; }
  #sheet .sheet-words { columns: 3; border: 1px solid #999; border-radius: 8px;
    padding: 12px 12px 12px 3em;
    font: 15px/1.9 ui-monospace, Consolas, monospace; }
}
.banner { border-color: var(--accent); }
#login, #pair {
  background: var(--panel);
  border: 1px solid var(--line);
  border-radius: 12px;
  padding: 20px;
}
.sub, .group, .login-fields { display: grid; gap: 12px; }
#login-form { border-top: 1px solid var(--line); padding-top: 12px; }
details > summary { cursor: pointer; color: var(--accent); font-weight: 500; }
details[open] > summary { margin-bottom: 12px; }
.segmented { display: flex; gap: 16px; flex-wrap: wrap; }
.segmented label, label.check { font-weight: 400; margin: 0; display: flex;
  gap: 8px; align-items: baseline; }
.pair-code, .pair-number {
  font: 600 24px/1.3 ui-monospace, "Cascadia Mono", Consolas, monospace;
  letter-spacing: .08em;
  user-select: all;
}
.pair-number { font-size: 32px; letter-spacing: .3em; }
#pair-qr { width: 200px; height: 200px; image-rendering: pixelated;
  background: #fff; border-radius: 8px; }
.meter { height: 4px; border-radius: 2px; background: var(--line);
  overflow: hidden; margin-top: -6px; }
.meter span { display: block; height: 100%; width: 0; background: var(--bad);
  transition: width .15s; }
.meter span.fair { background: var(--warn); }
.meter span.good { background: var(--ok); }

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

// Fills each .login-fields with the sign-in fields, ids prefixed.
for (const box of document.querySelectorAll('.login-fields')) {
  const prefix = box.dataset.prefix;
  box.append($('login-fields').content.cloneNode(true));
  for (const el of box.querySelectorAll('[data-field]')) {
    el.id = `${prefix}-${el.dataset.field}`;
  }
  for (const el of box.querySelectorAll('[data-for]')) {
    el.htmlFor = `${prefix}-${el.dataset.for}`;
  }
  const update = () => updateMeter(prefix);
  $(`${prefix}-login`).addEventListener('input', update);
  $(`${prefix}-password`).addEventListener('input', update);
  updateMeter(prefix);
}

const PROBLEMS = {
  tooShort: 'At least 15 characters.',
  tooLong: 'At most 256 characters.',
  common: 'Too common or too easy to guess.',
  containsLogin: 'Do not use your login name or the word Arcomium.',
};

async function updateMeter(prefix) {
  const login = $(`${prefix}-login`).value;
  const password = $(`${prefix}-password`).value;
  const meter = $(`${prefix}-meter`);
  const hint = $(`${prefix}-hint`);
  if (!password) {
    meter.style.width = '0';
    hint.textContent = 'At least 15 characters. A few unrelated words are ' +
        'easy to remember and hard to guess.';
    return;
  }
  const {problem} = await sendWithPromise('checkPassword', login, password);
  if ($(`${prefix}-password`).value !== password) return;
  const length = [...password].length;
  let width = Math.min(100, length / 25 * 100);
  let tone = '';
  let text = PROBLEMS[problem];
  if (problem === 'tooShort') {
    text = `${15 - length} more characters.`;
  } else if (problem) {
    width = 15;
  } else if (length < 20) {
    tone = 'fair';
    text = 'Good. Longer is stronger.';
  } else {
    tone = 'good';
    text = 'Strong.';
  }
  meter.className = tone;
  meter.style.width = `${width}%`;
  hint.textContent = text;
}

function loginFields(prefix) {
  return {
    login: $(`${prefix}-login`).value,
    password: $(`${prefix}-password`).value,
    confirm: $(`${prefix}-confirm`).value,
  };
}

let loginShown = false;

async function refreshLogin() {
  let state;
  try {
    state = await sendWithPromise('getLoginState');
  } catch (message) {
    $('login').hidden = true;
    return;
  }
  const set = state.onServer === 'this' || state.onServer === 'other';
  let text;
  switch (state.onServer) {
    case 'none':
      text = 'Not set up. Other devices join with the 24-word sync code. ' +
          'You can add an email or login name and a password, so they can ' +
          'join with those instead.';
      break;
    case 'this':
      text = `Set up as ${state.login}. Other devices can join with it and ` +
          'its password.';
      break;
    case 'other':
      text = 'Set up from another device. Setting it here replaces that ' +
          'email or login name and password.';
      break;
    default:
      text = 'Could not ask the server about it. ' + state.error;
  }
  $('login-text').textContent = text;
  $('login-set').textContent = set ? 'Change' : 'Set up';
  $('login-set').hidden = state.onServer === 'unknown';
  $('login-remove').hidden = !set;
  $('login').dataset.login = state.login;
  lastLogin = state.login;
  $('login').hidden = false;
}

let lastState = {server: ''};
let lastLogin = '';

function el(tag, text, className) {
  const e = document.createElement(tag);
  if (text !== undefined) e.textContent = text;
  if (className) e.className = className;
  return e;
}

// The recovery sheet: a page section that only prints. The print dialog
// prints it or saves it as a PDF; nothing leaves this device.
function printRecoverySheet() {
  const words = $('code').dataset.code.split(' ');
  const sheet = $('sheet');
  const rows = [['Sync server', lastState.server]];
  if (lastLogin) {
    rows.push(['Email or login name', lastLogin], ['Password', '']);
  }
  const table = el('table');
  for (const [name, value] of rows) {
    const tr = el('tr');
    tr.append(el('th', name), el('td', value, value ? '' : 'blank'));
    table.append(tr);
  }
  const list = el('ol', undefined, 'sheet-words');
  list.append(...words.map((w) => el('li', w)));
  const steps = el('ol');
  steps.append(
      el('li', 'In Arcomium, open Sync from the logo menu.'),
      el('li', 'Under Join a sync chain, enter the sync server above.'),
      el('li', 'Choose Sync code (24 words) and type the words in order; ' +
          'the first four letters of each word are enough.' +
          (lastLogin ? ' Or choose Email and password and sign in.' : '')),
      el('li', 'Restart Arcomium when it asks.'));
  sheet.replaceChildren(
      el('h1', 'Arcomium sync recovery sheet'),
      el('p', `Made on ${new Date().toLocaleDateString()}. Keep it ` +
          'somewhere safe: anyone with these words can read your synced ' +
          'data.', 'note'),
      table, el('h2', 'Sync code'), list, el('h2', 'To add a device'), steps);
  print();
}

// Pairing on the device that syncs.
let pairTimer = 0;

function drawQr(qr) {
  const canvas = $('pair-qr');
  canvas.hidden = !qr;
  if (!qr) return;
  const quiet = 4;
  const cells = qr.size + 2 * quiet;
  const scale = Math.floor(canvas.width / cells);
  const offset = Math.floor((canvas.width - cells * scale) / 2);
  const ctx = canvas.getContext('2d');
  ctx.fillStyle = '#fff';
  ctx.fillRect(0, 0, canvas.width, canvas.height);
  ctx.fillStyle = '#000';
  for (let y = 0; y < qr.size; y++) {
    for (let x = 0; x < qr.size; x++) {
      if (qr.modules[y * qr.size + x] === '1') {
        ctx.fillRect(offset + (x + quiet) * scale, offset + (y + quiet) * scale,
            scale, scale);
      }
    }
  }
}

function showPairError(text) {
  $('pair-error').textContent = text;
  $('pair-error').hidden = false;
}

function stopPairPolling() {
  clearTimeout(pairTimer);
  pairTimer = 0;
}

async function pollPairing() {
  let state;
  try {
    state = await sendWithPromise('pairingStatus');
  } catch (message) {
    state = {state: 'waiting'};
  }
  if ($('pair').hidden) return;
  if (state.expires) {
    $('pair-expires').textContent =
        `The code expires in ${Math.ceil(state.expires / 60)} min.`;
  }
  switch (state.state) {
    case 'waiting':
      pairTimer = setTimeout(pollPairing, 2000);
      return;
    case 'joined':
      $('pair-show').hidden = true;
      $('pair-name').textContent = state.name;
      $('pair-number').textContent = state.number;
      $('pair-ask').hidden = false;
      return;
    case 'wrongCode':
      $('pair-show').hidden = true;
      showPairError('The other device typed a wrong code, so the pairing ' +
          'was closed. Make a new code and try again.');
      return;
    default:
      $('pair-show').hidden = true;
      showPairError('The code expired. Make a new one.');
  }
}

$('pair-start').addEventListener('click', async () => {
  stopPairPolling();
  $('pair-error').hidden = true;
  $('pair-result').hidden = true;
  $('pair-ask').hidden = true;
  $('pair-show').hidden = true;
  $('pair').hidden = false;
  try {
    const {code, qr} = await sendWithPromise('startPairing');
    $('pair-code').textContent = code;
    drawQr(qr);
    $('pair-expires').textContent = 'The code expires in 5 min.';
    $('pair-show').hidden = false;
    pairTimer = setTimeout(pollPairing, 2000);
  } catch (message) {
    showPairError(String(message));
  }
});

$('pair-allow').addEventListener('click', async () => {
  $('pair-allow').disabled = true;
  try {
    await sendWithPromise('approvePairing');
    $('pair-ask').hidden = true;
    $('pair-result').textContent = `${$('pair-name').textContent} got the ` +
        'sync chain. It starts syncing after Arcomium restarts there.';
    $('pair-result').hidden = false;
  } catch (message) {
    showPairError(String(message));
  } finally {
    $('pair-allow').disabled = false;
  }
});

async function closePairing() {
  stopPairPolling();
  $('pair').hidden = true;
  await sendWithPromise('cancelPairing');
}
$('pair-deny').addEventListener('click', closePairing);
$('pair-close').addEventListener('click', closePairing);

// Pairing on the new device: waits until the other device allows it.
async function waitForPairing() {
  let state;
  try {
    state = await sendWithPromise('pairingJoinStatus');
  } catch (message) {
    state = {state: 'failed'};
  }
  if (state.state === 'waiting') {
    pairTimer = setTimeout(waitForPairing, 2000);
    return;
  }
  $('join-pair-wait').hidden = true;
  $('join').querySelector('button[type=submit]').disabled = false;
  if (state.state === 'cancelled') {
    $('join-error').textContent = 'The pairing was closed: the code was ' +
        'wrong, the other device denied it, or it expired.';
    $('join-error').hidden = false;
  } else if (state.state === 'failed') {
    $('join-error').textContent = 'The pairing failed. Make a new code on ' +
        'the other device.';
    $('join-error').hidden = false;
  }
}

function render(state) {
  lastState = state;
  const configured = state.server !== '';
  $('setup').hidden = configured;
  $('status').hidden = !configured || state.restartNeeded;
  if (!configured) {
    $('code').hidden = true;
  }

  const showLogin = configured && !state.restartNeeded;
  if (!showLogin) {
    $('login').hidden = true;
    $('pair').hidden = true;
    loginShown = false;
  } else if (!loginShown) {
    loginShown = true;
    refreshLogin();
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
    const fields = loginFields('create');
    const withLogin = $('create-login-box').open &&
        (fields.login !== '' || fields.password !== '');
    const {code} = await sendWithPromise('createChain',
        $('create-server').value, withLogin ? fields : null);
    lastLogin = withLogin ? fields.login.trim().toLowerCase() : '';
    showCode(code);
  });
});

$('join').addEventListener('submit', (e) => {
  e.preventDefault();
  const via = document.querySelector('input[name=join-with]:checked').value;
  if (via === 'pair') {
    const button = e.target.querySelector('button[type=submit]');
    submit(e.target, 'join-error', async () => {
      const {number} = await sendWithPromise('joinWithPairing',
          $('join-server').value, $('join-pair-code').value);
      $('join-pair-number').textContent = number;
      $('join-pair-wait').hidden = false;
      stopPairPolling();
      pairTimer = setTimeout(waitForPairing, 2000);
    }).then(() => {
      // Stays disabled while the other device decides.
      if (!$('join-pair-wait').hidden) button.disabled = true;
    });
    return;
  }
  submit(e.target, 'join-error', () => via === 'login' ?
      sendWithPromise('joinWithLogin', $('join-server').value,
          $('join-login').value, $('join-password').value) :
      sendWithPromise('joinChain', $('join-server').value, $('join-code').value));
});

for (const radio of document.querySelectorAll('input[name=join-with]')) {
  radio.addEventListener('change', () => {
    if (!radio.checked) return;
    $('join-with-login').hidden = radio.value !== 'login';
    $('join-with-code').hidden = radio.value !== 'code';
    $('join-with-pair').hidden = radio.value !== 'pair';
    $('join-error').hidden = true;
  });
}

$('login-set').addEventListener('click', () => {
  $('login-login').value = $('login').dataset.login || '';
  $('login-password').value = '';
  $('login-confirm').value = '';
  updateMeter('login');
  $('login-error').hidden = true;
  $('login-remove-confirm').hidden = true;
  $('login-form').hidden = false;
  $('login-actions').hidden = true;
  $('login-login').focus();
});
$('login-cancel').addEventListener('click', () => {
  $('login-form').hidden = true;
  $('login-actions').hidden = false;
});
$('login-form').addEventListener('submit', (e) => {
  e.preventDefault();
  submit(e.target, 'login-error', async () => {
    await sendWithPromise('setLogin', loginFields('login'));
    $('login-form').hidden = true;
    $('login-actions').hidden = false;
    await refreshLogin();
  });
});
$('login-remove').addEventListener('click', () => {
  $('login-remove-error').hidden = true;
  $('login-remove-confirm').hidden = false;
});
$('login-remove-no').addEventListener('click', () => {
  $('login-remove-confirm').hidden = true;
});
$('login-remove-yes').addEventListener('click', async () => {
  $('login-remove-yes').disabled = true;
  try {
    await sendWithPromise('removeLogin');
    $('login-remove-confirm').hidden = true;
    await refreshLogin();
  } catch (message) {
    $('login-remove-error').textContent = String(message);
    $('login-remove-error').hidden = false;
  } finally {
    $('login-remove-yes').disabled = false;
  }
});

$('show-code').addEventListener('click', async () => {
  try {
    const {code} = await sendWithPromise('showCode');
    showCode(code);
  } catch (message) {
    $('status-text').textContent = String(message);
  }
});

$('recovery-sheet').addEventListener('click', printRecoverySheet);

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
