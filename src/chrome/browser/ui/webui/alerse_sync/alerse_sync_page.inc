// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The files of chrome://alerse-sync, served by AlerseSyncUI.

#ifndef CHROME_BROWSER_UI_WEBUI_ALERSE_SYNC_ALERSE_SYNC_PAGE_INC_
#define CHROME_BROWSER_UI_WEBUI_ALERSE_SYNC_ALERSE_SYNC_PAGE_INC_

inline constexpr char kAlerseSyncHtml[] = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>Sync</title>
<link rel="stylesheet" href="base.css">
<link rel="stylesheet" href="alerse_sync.css">
<script type="module" src="alerse_sync.js"></script>
</head>
<body>
<main>
  <header>
    <h1>Sync</h1>
    <p class="lede">Keep bookmarks, tabs, Spaces, passwords and settings the
      same on all your devices. Everything is encrypted on this device before
      it leaves it: the server stores only data it cannot read.</p>
  </header>

  <section id="legacy" class="banner" hidden>
    <p>Sync works differently now: an account opens with your 24 words and a
      password, and you can remove a lost device. Your earlier sync was
      turned off on this device. Your data is still here; set sync up again
      below, and it is uploaded to the new account.</p>
    <button id="legacy-ok">OK</button>
  </section>

  <section id="restart" class="banner" hidden>
    <p id="restart-text"></p>
    <button id="restart-button" class="primary">Restart Alerse</button>
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
    <div id="summary" class="summary" hidden>
      <h3 id="summary-title"></h3>
      <ul id="summary-list" class="space-list"></ul>
    </div>
    <div class="actions">
      <button id="devices-open">Devices</button>
      <button id="words-open">Show the 24 words</button>
      <button id="leave" class="quiet">Stop syncing on this device</button>
      <button id="delete-open" class="quiet">Delete account</button>
    </div>
    <div id="leave-confirm" class="confirm" hidden>
      <p>This device stops syncing and forgets the account. Its data stays
        here, and the account keeps syncing on your other devices.</p>
      <div class="actions">
        <button id="leave-yes" class="danger">Stop syncing</button>
        <button id="leave-no">Cancel</button>
      </div>
    </div>
    <form id="delete-form" class="confirm" novalidate hidden>
      <p>The account and everything synced to it are deleted from the server.
        Every device stops syncing. The data on each device stays there.</p>
      <label for="delete-password">Password</label>
      <input id="delete-password" type="password" autocomplete="current-password">
      <p id="delete-error" class="error" role="alert" hidden></p>
      <div class="actions">
        <button id="delete-yes" class="danger" type="submit">Delete account</button>
        <button id="delete-no" type="button">Cancel</button>
      </div>
    </form>
  </section>

  <section id="devices" class="panel" hidden>
    <h2>Devices</h2>
    <p class="note">Remove a device you lost or gave away: it is refused from
      its next request on. Its own copy of the data stays on it.</p>
    <ul id="device-list" class="device-list"></ul>
    <form id="remove-form" class="sub" novalidate hidden>
      <p>Remove <strong id="remove-name"></strong>? Enter the password.</p>
      <input id="remove-password" type="password" autocomplete="current-password"
             aria-label="Password">
      <div class="actions">
        <button id="remove-yes" class="danger" type="submit">Remove</button>
        <button id="remove-no" type="button">Cancel</button>
      </div>
    </form>
    <p id="devices-error" class="error" role="alert" hidden></p>
    <div class="actions">
      <button id="devices-close" class="quiet">Close</button>
    </div>
  </section>

  <section id="words" class="panel" hidden>
    <h2>Your 24 words</h2>
    <p class="note">A new device needs these words and your password. Keep them
      on paper or in a password manager, apart from the password. Nobody can
      reset either: if both your devices and the words are lost, the synced
      data is gone.</p>
    <ol id="word-list" class="words"></ol>
    <div class="actions">
      <button id="print-sheet">Print recovery sheet</button>
      <button id="words-close" class="quiet">Close</button>
    </div>
    <div class="pm">
      <h3>Save to a password manager</h3>
      <p class="note">Password manager extensions cannot reach this page, so the
        entry goes in by import. It holds the words as the login<span
        id="pm-with-password"> and the password</span>, with the server.</p>
      <div class="actions">
        <button id="pm-bitwarden">Copy for Bitwarden</button>
        <button id="pm-csv">Download CSV</button>
      </div>
      <p id="pm-done" class="note" role="status" hidden></p>
    </div>
    <div id="words-saved-box" hidden>
      <label class="check"><input id="words-saved" type="checkbox"> I saved the
        words</label>
    </div>
  </section>

  <section id="setup" hidden>
    <fieldset class="card server-choice">
      <legend>Sync server</legend>
      <p class="note">A server of your own is the better choice: your data
        stays with you. Running one takes Docker or Kubernetes and a few
        minutes; the <a href="https://github.com/VizzleTF/alerse-sync#run-your-own-server"
        target="_blank" rel="noopener">guide</a> shows how.</p>
      <label class="check"><input type="radio" name="server" value="own">
        My own server (recommended)</label>
      <input id="own-server" type="url" inputmode="url" autocomplete="url"
             placeholder="https://sync.example.com" spellcheck="false" hidden
             aria-label="Your server's address">
      <label class="check"><input type="radio" name="server" value="alerse"
        checked> The Alerse server (<span id="default-server"></span>)</label>
      <p class="note">The Alerse server is run by Alerse's maintainer as a
        favour, free and without any guarantee: it may stop, and its operator
        takes no responsibility for the data on it. Everything is encrypted on
        your devices, so it cannot read your data.
        <a href="https://github.com/VizzleTF/alerse-sync/blob/main/docs/terms.md"
        target="_blank" rel="noopener">Terms</a> ·
        <a href="https://github.com/VizzleTF/alerse-sync/blob/main/docs/privacy.md"
        target="_blank" rel="noopener">Privacy</a></p>
    </fieldset>

    <form id="create" class="card" novalidate>
      <h2>Create an account</h2>
      <p class="note">Alerse makes 24 words for you. A new device needs the
        words and this password; neither alone opens the account.</p>
      <label for="create-password">Password</label>
      <input id="create-password" type="password" autocomplete="new-password"
             aria-describedby="create-hint">
      <div class="meter"><span id="create-meter"></span></div>
      <p id="create-hint" class="note">At least 6 characters. The 24 words are
        the other half: without them nobody can try the password.</p>
      <label for="create-confirm">Password again</label>
      <input id="create-confirm" type="password" autocomplete="new-password">
      <p id="create-error" class="error" role="alert" hidden></p>
      <button id="create-button" class="primary" type="submit">Create account</button>
    </form>

    <form id="signin" class="card" novalidate>
      <h2>Sign in</h2>
      <p class="note">On another device of yours: the 24 words and the
        password.</p>
      <label for="signin-words">24 words</label>
      <textarea id="signin-words" rows="3" spellcheck="false"
                autocomplete="username"></textarea>
      <label for="signin-password">Password</label>
      <input id="signin-password" type="password" autocomplete="current-password">
      <p id="signin-error" class="error" role="alert" hidden></p>
      <button id="signin-button" class="primary" type="submit">Sign in</button>
    </form>
  </section>
</main>

<section id="sheet" aria-hidden="true">
  <h1>Alerse recovery sheet</h1>
  <p class="note">Keep this sheet somewhere safe, apart from your password.
    Anyone with the words and the password can read your synced data.</p>
  <table>
    <tr><th>Server</th><td id="sheet-server"></td></tr>
    <tr><th>Made</th><td id="sheet-date"></td></tr>
    <tr><th>Password</th><td class="blank"></td></tr>
  </table>
  <ol id="sheet-words" class="sheet-words"></ol>
  <p>To add a device: install Alerse, open Sync, pick the same server, and
    sign in with the 24 words and the password.</p>
</section>
</body>
</html>
)HTML";

inline constexpr char kAlerseSyncCss[] = R"CSS(
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
.panel {
  background: var(--panel);
  border: 1px solid var(--line);
  border-radius: var(--radius-lg);
  padding: 20px;
}
.sub { display: grid; gap: 12px; border-top: 1px solid var(--line);
  padding-top: 12px; }
fieldset.server-choice { display: grid; gap: 8px; }
fieldset.server-choice legend { font-weight: 600; padding: 0 4px; }
label.check { font-weight: 400; margin: 0; display: flex; gap: 8px;
  align-items: baseline; }
.meter { height: 4px; border-radius: 2px; background: var(--line);
  overflow: hidden; margin-top: -6px; }
.meter span { display: block; height: 100%; width: 0; background: var(--bad);
  transition: width .15s; }
.meter span.fair { background: var(--warn); }
.meter span.good { background: var(--ok); }
.status-line { display: flex; align-items: center; gap: 8px; font-weight: 500; }
.dot { width: 8px; height: 8px; border-radius: 50%; background: var(--muted);
  flex: none; }
.dot.ok { background: var(--ok); }
.dot.warn { background: var(--warn); }
.dot.bad { background: var(--bad); }
dl { display: grid; grid-template-columns: max-content 1fr; gap: 4px 16px;
  margin: 0; }
dt { color: var(--muted); }
dd { margin: 0; overflow-wrap: anywhere; }
.confirm { display: grid; gap: 12px; border-top: 1px solid var(--line);
  padding-top: 12px; }
.summary { display: grid; gap: 8px; border-top: 1px solid var(--line);
  padding-top: 12px; margin-top: 12px; }
.summary h3 { margin: 0; font-size: 15px; }
.space-list { list-style: none; margin: 0; padding: 0; display: grid; gap: 6px; }
.space-list li { display: grid; grid-template-columns: 8px 1fr auto; gap: 4px 10px;
  align-items: baseline; }
.space-list .dot { align-self: center; }
.space-list .name { font-weight: 500; overflow-wrap: anywhere; }
.space-list .tabs { color: var(--muted); font-size: 13px; text-align: right; }
.space-list .others { grid-column: 2 / 4; color: var(--muted); font-size: 13px; }
.device-list { list-style: none; margin: 0; padding: 0; display: grid;
  gap: 6px; }
.pm { display: grid; gap: 8px; border-top: 1px solid var(--line);
  padding-top: 12px; margin-top: 12px; }
.pm h3 { margin: 0; font-size: 15px; }
.device-list li { display: flex; align-items: center; gap: 12px;
  background: var(--bg); border-radius: 8px; padding: 8px 12px; }
.device-list .name { font-weight: 500; flex: 1; overflow-wrap: anywhere; }
.device-list .seen { color: var(--muted); font-size: 13px; }
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

inline constexpr char kAlerseSyncJs[] = R"JS(
import {addWebUiListener, sendWithPromise} from 'chrome://resources/js/cr.js';

const $ = (id) => document.getElementById(id);
let state = {};
// Set while the words of a new account are on screen: the restart waits
// until the user says they are saved.
let newAccount = false;
// The words on screen, and the password of an account created just now: the
// browser keeps no password, so only then can the entry carry it.
let shownWords = '';
let newPassword = '';

function showError(id, message) {
  const el = $(id);
  el.textContent = message || '';
  el.hidden = !message;
}

function chosenServer() {
  const own = document.querySelector('input[name=server]:checked').value ===
      'own';
  return own ? $('own-server').value.trim() : state.defaultServer;
}

function busy(button, on) {
  button.disabled = on;
}

function fillWords(list, words) {
  list.replaceChildren(...words.split(' ').map((word) => {
    const li = document.createElement('li');
    li.textContent = word;
    return li;
  }));
}

function showWords(words, fresh) {
  shownWords = words;
  $('pm-with-password').hidden = !newPassword;
  $('pm-done').hidden = true;
  fillWords($('word-list'), words);
  fillWords($('sheet-words'), words);
  $('sheet-server').textContent = state.server || chosenServer();
  $('sheet-date').textContent = new Date().toLocaleDateString();
  $('words').hidden = false;
  $('words-saved-box').hidden = !fresh;
  $('words-close').hidden = fresh;
}

function formatSeen(value) {
  if (!value || value.startsWith('0001')) {
    return 'never';
  }
  return new Date(value).toLocaleString();
}

const kStatus = {
  active: ['Syncing', 'ok'],
  starting: ['Starting…', 'warn'],
  stopped: ['Stopped', 'warn'],
  refused: ['The server refused this device. It may have been removed.', 'bad'],
  wrongKey: ['The account\'s data does not open with this device\'s key.',
             'bad'],
  error: ['Sync stopped with an error.', 'bad'],
  off: ['Not syncing yet', 'warn'],
};

// Chromium's local sync mode keeps a refused device looking active, so the
// page asks the server whether it still knows this device.
let checkedDevice = false;
let removed = false;
async function checkDevice() {
  try {
    await sendWithPromise('getDevices');
  } catch (error) {
    removed = String(error).includes('removed');
    render(state);
  }
}

function render(next) {
  state = next;
  const setUp = !!state.server;
  $('legacy').hidden = !state.legacyReset || setUp;
  $('default-server').textContent = state.defaultServer;
  $('setup').hidden = setUp;
  $('status').hidden = !setUp;
  if (setUp) {
    const [text, tone] = kStatus[state.status] || kStatus.off;
    $('status-text').textContent = text;
    $('status-dot').className = 'dot ' + tone;
    $('status-server').textContent = state.server;
    $('status-synced').textContent =
        state.lastSynced ? new Date(state.lastSynced).toLocaleString() : '—';
    // The account is saved, but the sync engine takes it only on a restart.
    if (state.restartNeeded) {
      $('status-text').textContent = 'Starts after a restart of Alerse';
      $('status-dot').className = 'dot warn';
    }
  } else {
    $('devices').hidden = true;
  }
  if (setUp && !checkedDevice) {
    checkedDevice = true;
    checkDevice();
  }
  if (setUp && removed) {
    $('status-text').textContent =
        'This device was removed from the account. Stop syncing on it, then ' +
        'sign in again if it should sync.';
    $('status-dot').className = 'dot bad';
  }
  const restart = state.restartNeeded && !(newAccount && !$('words-saved').checked);
  $('restart').hidden = !restart;
  $('restart-text').textContent = setUp ?
      'Restart Alerse to start syncing in every Space.' :
      'Restart Alerse to stop syncing on this device.';
}

// Setup.
document.querySelectorAll('input[name=server]').forEach((radio) => {
  radio.addEventListener('change', () => {
    $('own-server').hidden =
        document.querySelector('input[name=server]:checked').value !== 'own';
  });
});

let checkTimer = 0;
$('create-password').addEventListener('input', () => {
  clearTimeout(checkTimer);
  const password = $('create-password').value;
  const meter = $('create-meter');
  const length = [...password].length;
  meter.style.width = Math.min(100, length / 12 * 100) + '%';
  checkTimer = setTimeout(async () => {
    const {problem} = await sendWithPromise('checkPassword', password);
    meter.className = problem ? '' : (length >= 12 ? 'good' : 'fair');
  }, 200);
});

$('create').addEventListener('submit', async (e) => {
  e.preventDefault();
  showError('create-error', '');
  busy($('create-button'), true);
  try {
    const {words} = await sendWithPromise(
        'createAccount', chosenServer(), $('create-password').value,
        $('create-confirm').value);
    newPassword = $('create-password').value;
    $('create-password').value = $('create-confirm').value = '';
    newAccount = true;
    showWords(words, true);
  } catch (error) {
    showError('create-error', error);
  } finally {
    busy($('create-button'), false);
  }
});

$('signin').addEventListener('submit', async (e) => {
  e.preventDefault();
  showError('signin-error', '');
  busy($('signin-button'), true);
  try {
    await sendWithPromise('signIn', chosenServer(), $('signin-words').value,
                          $('signin-password').value);
    $('signin-words').value = $('signin-password').value = '';
  } catch (error) {
    showError('signin-error', error);
  } finally {
    busy($('signin-button'), false);
  }
});

$('legacy-ok').addEventListener('click', () => sendWithPromise('dismissLegacy'));
$('restart-button').addEventListener('click', () => chrome.send('restart'));

// Words.
$('words-open').addEventListener('click', async () => {
  const {words} = await sendWithPromise('showWords');
  showWords(words, false);
});
$('words-close').addEventListener('click', () => {
  $('words').hidden = true;
  fillWords($('word-list'), '');
  shownWords = newPassword = '';
});

// Password manager entries, by import: Bitwarden's CSV through the
// clipboard, which its import page takes pasted, and Chrome's CSV as a file,
// which 1Password, Proton Pass, KeePassXC, Edge and Brave import.
function csvRow(fields) {
  return fields.map((f) => '"' + String(f).replaceAll('"', '""') + '"')
      .join(',');
}
function entry() {
  const server = state.server || chosenServer();
  const notes = 'Alerse sync: on a new device open Sync, pick ' + server +
      ', sign in with these 24 words and the password.' +
      (newPassword ? '' : ' Add the password to this entry.');
  return {name: 'Alerse sync', server, words: shownWords,
          password: newPassword, notes};
}
let clipboardTimer = 0;
$('pm-bitwarden').addEventListener('click', async () => {
  const e = entry();
  const text = csvRow(['folder', 'favorite', 'type', 'name', 'notes',
                       'fields', 'reprompt', 'login_uri', 'login_username',
                       'login_password', 'login_totp']) + '\n' +
      csvRow(['', '', 'login', e.name, e.notes, '', '1', e.server, e.words,
              e.password, '']) + '\n';
  await navigator.clipboard.writeText(text);
  // The entry holds the account's secrets: they leave the clipboard soon.
  clearTimeout(clipboardTimer);
  clipboardTimer = setTimeout(async () => {
    try {
      if (await navigator.clipboard.readText() === text) {
        await navigator.clipboard.writeText('');
      }
    } catch (error) {}
  }, 120000);
  $('pm-done').textContent = 'Copied. In the Bitwarden extension open ' +
      'Settings, Vault options, Import items; pick the file format ' +
      '"Bitwarden (csv)", paste into the text box and press Import. In the ' +
      'Bitwarden web app it is Tools, Import data. The clipboard is cleared ' +
      'in 2 minutes.';
  $('pm-done').hidden = false;
});
$('pm-csv').addEventListener('click', () => {
  const e = entry();
  const text = csvRow(['name', 'url', 'username', 'password', 'note']) + '\n' +
      csvRow([e.name, e.server, e.words, e.password, e.notes]) + '\n';
  const link = document.createElement('a');
  link.href = URL.createObjectURL(new Blob([text], {type: 'text/csv'}));
  link.download = 'alerse-sync.csv';
  link.click();
  setTimeout(() => URL.revokeObjectURL(link.href), 10000);
  $('pm-done').textContent = 'Saved alerse-sync.csv. Import it in the ' +
      'password manager, then delete the file: it holds the words' +
      (e.password ? ' and the password.' : '.');
  $('pm-done').hidden = false;
});
$('words-saved').addEventListener('change', () => render(state));
$('print-sheet').addEventListener('click', () => window.print());

// Devices.
let removing = null;
async function loadDevices() {
  showError('devices-error', '');
  try {
    const {devices} = await sendWithPromise('getDevices');
    $('device-list').replaceChildren(...devices.map((device) => {
      const li = document.createElement('li');
      const name = document.createElement('span');
      name.className = 'name';
      name.textContent = device.name + (device.current ? ' (this device)' : '');
      const seen = document.createElement('span');
      seen.className = 'seen';
      seen.textContent = 'Last seen ' + formatSeen(device.lastSeen);
      li.append(name, seen);
      if (!device.current) {
        const remove = document.createElement('button');
        remove.className = 'quiet';
        remove.textContent = 'Remove';
        remove.addEventListener('click', () => {
          removing = device;
          $('remove-name').textContent = device.name;
          $('remove-form').hidden = false;
          $('remove-password').focus();
        });
        li.append(remove);
      }
      return li;
    }));
  } catch (error) {
    showError('devices-error', error);
  }
}
$('devices-open').addEventListener('click', () => {
  $('devices').hidden = false;
  loadDevices();
});
$('devices-close').addEventListener('click', () => {
  $('devices').hidden = true;
});
$('remove-no').addEventListener('click', () => {
  $('remove-form').hidden = true;
  $('remove-password').value = '';
});
$('remove-form').addEventListener('submit', async (e) => {
  e.preventDefault();
  busy($('remove-yes'), true);
  try {
    await sendWithPromise('removeDevice', removing.id,
                          $('remove-password').value);
    $('remove-form').hidden = true;
    $('remove-password').value = '';
    loadDevices();
  } catch (error) {
    showError('devices-error', error);
  } finally {
    busy($('remove-yes'), false);
  }
});

// Leaving and deleting.
$('leave').addEventListener('click', () => {
  $('leave-confirm').hidden = false;
});
$('leave-no').addEventListener('click', () => {
  $('leave-confirm').hidden = true;
});
$('leave-yes').addEventListener('click', async () => {
  await sendWithPromise('leave');
  $('leave-confirm').hidden = true;
});
$('delete-open').addEventListener('click', () => {
  $('delete-form').hidden = false;
  $('delete-password').focus();
});
$('delete-no').addEventListener('click', () => {
  $('delete-form').hidden = true;
  $('delete-password').value = '';
});
$('delete-form').addEventListener('submit', async (e) => {
  e.preventDefault();
  showError('delete-error', '');
  busy($('delete-yes'), true);
  try {
    await sendWithPromise('deleteAccount', $('delete-password').value);
    $('delete-form').hidden = true;
    $('delete-password').value = '';
  } catch (error) {
    showError('delete-error', error);
  } finally {
    busy($('delete-yes'), false);
  }
});

addWebUiListener('state-changed', render);
sendWithPromise('getState').then((state) => {
  render(state);
  refreshSummary();
});

// What syncs: every Space, whether it syncs, and its tabs here and on the
// other devices, as this device last got them. Local data only, so it is
// asked for often.
function plural(n, one, many) {
  return n + ' ' + (n === 1 ? one : many);
}
function renderSummary(spaces) {
  const list = $('summary-list');
  list.textContent = '';
  let tabs = 0;
  for (const space of spaces) {
    const li = document.createElement('li');
    const dot = document.createElement('span');
    dot.className = 'dot ' + (space.syncing ? 'ok' : 'warn');
    dot.title = space.syncing ? 'Syncing' : 'Not syncing yet';
    const name = document.createElement('span');
    name.className = 'name';
    name.textContent = space.name;
    const here = document.createElement('span');
    here.className = 'tabs';
    here.textContent = space.tabs === undefined ? '' :
        plural(space.tabs, 'tab', 'tabs') + ' here';
    li.append(dot, name, here);
    if (space.devices.length) {
      const others = document.createElement('span');
      others.className = 'others';
      others.textContent = space.devices.map(
          (d) => d.name + ': ' + plural(d.tabs, 'tab', 'tabs')).join(' · ');
      li.append(others);
    }
    list.append(li);
    tabs += space.tabs || 0;
  }
  $('summary-title').textContent = 'Syncing ' +
      plural(spaces.length, 'Space', 'Spaces') + ' with ' +
      plural(tabs, 'tab', 'tabs') + ' from this device';
  $('summary').hidden = spaces.length === 0;
}
async function refreshSummary() {
  if ($('status').hidden || document.hidden) {
    return;
  }
  try {
    renderSummary(await sendWithPromise('getSummary'));
  } catch (error) {
    $('summary').hidden = true;
  }
}
addWebUiListener('state-changed', () => refreshSummary());
document.addEventListener('visibilitychange', refreshSummary);
setInterval(refreshSummary, 5000);
)JS";

#endif  // CHROME_BROWSER_UI_WEBUI_ALERSE_SYNC_ALERSE_SYNC_PAGE_INC_
