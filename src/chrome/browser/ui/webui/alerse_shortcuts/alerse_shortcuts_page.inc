// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The files of chrome://alerse-shortcuts, served by AlerseShortcutsUI.

#ifndef CHROME_BROWSER_UI_WEBUI_ALERSE_SHORTCUTS_ALERSE_SHORTCUTS_PAGE_INC_
#define CHROME_BROWSER_UI_WEBUI_ALERSE_SHORTCUTS_ALERSE_SHORTCUTS_PAGE_INC_

inline constexpr char kAlerseShortcutsHtml[] = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>Keyboard shortcuts</title>
<link rel="stylesheet" href="base.css">
<link rel="stylesheet" href="alerse_shortcuts.css">
<script type="module" src="alerse_shortcuts.js"></script>
</head>
<body>
<main>
  <header>
    <h1>Keyboard shortcuts</h1>
    <p class="lede" id="lede-edit" hidden>Click a shortcut and press the keys
      you want for it. A key you give one command leaves the command that had
      it. Every window takes a change at once.</p>
    <p class="lede" id="lede-mac" hidden>On macOS, set shortcuts for any item
      of Alerse's menus in System Settings: Keyboard, Keyboard Shortcuts, App
      Shortcuts, then add one for Alerse with the menu item's name. The list
      below shows the keys Alerse has now.</p>
    <p id="mac-row" hidden><button id="system" type="button">Open System
      Settings</button></p>
  </header>
  <section class="card">
    <ul id="commands" class="rows"></ul>
    <p id="reset-row" hidden><button id="reset-all" type="button"
        class="quiet">Reset all</button></p>
  </section>
</main>
</body>
</html>
)HTML";

inline constexpr char kAlerseShortcutsCss[] = R"CSS(
.rows { list-style: none; margin: 0; padding: 0; }
.rows li { display: flex; align-items: center; gap: 12px; padding: 8px 0;
  border-top: 1px solid var(--line); }
.rows li:first-child { border-top: none; }
.rows .name { flex: 1; }
.rows .custom-mark { color: var(--accent); font-size: 12px; }
.keys { display: flex; gap: 6px; }
kbd { font: 12px ui-monospace, Menlo, Consolas, monospace; padding: 3px 8px;
  border-radius: 6px; border: 1px solid var(--line); background: var(--panel);
  white-space: nowrap; }
button.key { min-width: 120px; }
button.key.recording { outline: 2px solid var(--accent); }
.none { color: var(--muted); font-size: 12px; }
)CSS";

inline constexpr char kAlerseShortcutsJs[] = R"JS(
import {sendWithPromise} from 'chrome://resources/js/cr.js';

const $ = (id) => document.getElementById(id);
// ui::EventFlags.
const SHIFT = 1 << 1, CONTROL = 1 << 2, ALT = 1 << 3, COMMAND = 1 << 4;
const MODIFIER_KEYS = new Set([16, 17, 18, 91, 92, 93, 224]);
let recording = null;

function keyRow(command, editable) {
  const li = document.createElement('li');
  const name = document.createElement('span');
  name.className = 'name';
  name.textContent = command.name;
  if (command.custom) {
    const mark = document.createElement('span');
    mark.className = 'custom-mark';
    mark.textContent = ' · yours';
    name.append(mark);
  }
  const keys = document.createElement('span');
  keys.className = 'keys';
  for (const text of command.keys) {
    const kbd = document.createElement('kbd');
    kbd.textContent = text;
    keys.append(kbd);
  }
  if (!command.keys.length) {
    const none = document.createElement('span');
    none.className = 'none';
    none.textContent = 'None';
    keys.append(none);
  }
  li.append(name, keys);
  if (!editable) return li;

  const change = document.createElement('button');
  change.type = 'button';
  change.className = 'key';
  change.textContent = 'Change';
  change.addEventListener('click', () => startRecording(command, change));
  const clear = document.createElement('button');
  clear.type = 'button';
  clear.className = 'quiet';
  clear.textContent = 'Remove';
  clear.title = 'No key for this command';
  clear.addEventListener('click', async () =>
      render(await sendWithPromise('setShortcut', command.id, 0, 0)));
  li.append(change, clear);
  if (command.custom) {
    const reset = document.createElement('button');
    reset.type = 'button';
    reset.className = 'quiet';
    reset.textContent = 'Reset';
    reset.addEventListener('click', async () =>
        render(await sendWithPromise('resetShortcut', command.id)));
    li.append(reset);
  }
  return li;
}

function startRecording(command, button) {
  stopRecording();
  recording = {command, button};
  button.classList.add('recording');
  button.textContent = 'Press keys…';
}

function stopRecording() {
  if (!recording) return;
  recording.button.classList.remove('recording');
  recording.button.textContent = 'Change';
  recording = null;
}

document.addEventListener('keydown', async (e) => {
  if (!recording) return;
  e.preventDefault();
  e.stopPropagation();
  if (e.key === 'Escape') {
    stopRecording();
    return;
  }
  if (MODIFIER_KEYS.has(e.keyCode)) return;
  const mods = (e.shiftKey ? SHIFT : 0) | (e.ctrlKey ? CONTROL : 0) |
      (e.altKey ? ALT : 0) | (e.metaKey ? COMMAND : 0);
  // A plain key would take over typing; function keys may stand alone.
  const fkey = e.keyCode >= 112 && e.keyCode <= 135;
  if (!mods && !fkey) {
    recording.button.textContent = 'Add Ctrl or Alt';
    return;
  }
  const {command} = recording;
  recording = null;
  render(await sendWithPromise('setShortcut', command.id, e.keyCode, mods));
}, true);

function render(state) {
  $('lede-edit').hidden = !state.editable;
  $('lede-mac').hidden = state.editable;
  $('mac-row').hidden = state.editable;
  $('reset-row').hidden =
      !state.editable || !state.commands.some((c) => c.custom);
  const list = $('commands');
  list.replaceChildren(
      ...state.commands.map((c) => keyRow(c, state.editable)));
}

$('system').addEventListener('click', () =>
    chrome.send('openSystemShortcuts'));
$('reset-all').addEventListener('click', async () =>
    render(await sendWithPromise('resetShortcut', 0)));
sendWithPromise('getState').then(render);
)JS";

#endif  // CHROME_BROWSER_UI_WEBUI_ALERSE_SHORTCUTS_ALERSE_SHORTCUTS_PAGE_INC_
