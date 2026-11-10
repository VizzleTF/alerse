// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The files of chrome://alerse-import, served by AlerseImportUI. Its text
// comes from alerse_strings.grdp through strings.m.js; elements name their
// string in data-i18n.

#ifndef CHROME_BROWSER_UI_WEBUI_ALERSE_IMPORT_ALERSE_IMPORT_PAGE_INC_
#define CHROME_BROWSER_UI_WEBUI_ALERSE_IMPORT_ALERSE_IMPORT_PAGE_INC_

inline constexpr char kAlerseImportHtml[] = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title data-i18n="title"></title>
<link rel="stylesheet" href="base.css">
<link rel="stylesheet" href="alerse_import.css">
<script type="module" src="alerse_import.js"></script>
</head>
<body>
<main>
  <header>
    <h1 data-i18n="title"></h1>
    <p class="lede" data-i18n="lede"></p>
  </header>

  <p id="searching" class="note" data-i18n="searching"></p>
  <section id="none" class="card" hidden>
    <p data-i18n="nothingFound"></p>
  </section>

  <section id="sources" hidden>
    <ul id="source-list" class="sources"></ul>
    <p class="note" data-i18n="replaces"></p>
  </section>

  <section class="card">
    <a id="other" class="other" href="chrome://settings/importData"
        data-i18n="otherLink"></a>
    <p data-i18n="otherHint"></p>
  </section>
</main>

<template id="source-template">
  <li class="source card">
    <div class="source-head">
      <div class="source-name">
        <h2 class="browser"></h2>
        <span class="profile note"></span>
      </div>
      <button class="primary import"></button>
    </div>
    <p class="summary"></p>
    <p class="hint note"></p>
    <p class="result" role="status" hidden></p>
    <p class="error" role="alert" hidden></p>
  </li>
</template>
</body>
</html>
)HTML";

inline constexpr char kAlerseImportCss[] = R"CSS(
.sources { list-style: none; margin: 0; padding: 0; display: grid; gap: 12px; }
.source { gap: 8px; }
.source-head { display: flex; align-items: center; gap: 12px; }
.source-name { flex: 1; display: flex; align-items: baseline; gap: 8px; flex-wrap: wrap; min-width: 0; }
.source-name h2 { margin: 0; overflow-wrap: anywhere; }
.summary { font-variant-numeric: tabular-nums; }
.source > .result { color: var(--ok); }
.source > .error { color: var(--bad); }
.other { color: var(--accent); font-weight: 500; text-decoration: none; justify-self: start; }
.other:hover { text-decoration: underline; }
.other:focus-visible { outline: 2px solid var(--accent); outline-offset: 2px; border-radius: 4px; }
.card > p { color: var(--muted); }
)CSS";

inline constexpr char kAlerseImportJs[] = R"JS(
import './strings.m.js';
import {sendWithPromise} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

const $ = (id) => document.getElementById(id);
const t = (name, ...args) => loadTimeData.getStringF(name, ...args);

for (const element of document.querySelectorAll('[data-i18n]')) {
  element.textContent = loadTimeData.getString(element.dataset.i18n);
}

const ERRORS = {
  busy: 'errorBusy',
  notReady: 'errorNotReady',
  readFailed: 'errorRead',
  unknown: 'errorUnknown',
};

function resultText(source, result) {
  if (source.kind !== 'arc') {
    return t('bookmarksDone', result.links, result.folder);
  }
  let text = t('arcDone', result.spaces, result.links, result.folders);
  if (result.createdSpaces > 0) {
    text += ' ' + t('spacesCreated', result.createdSpaces);
  }
  if (result.pendingSpaces > 0) {
    text += ' ' + t('spacesPending', result.pendingSpaces);
  }
  return text;
}

function renderSource(source) {
  const item = $('source-template').content.firstElementChild.cloneNode(true);
  const button = item.querySelector('.import');
  const result = item.querySelector('.result');
  const error = item.querySelector('.error');
  item.querySelector('.browser').textContent = source.browser;
  item.querySelector('.profile').textContent = source.profile;
  item.querySelector('.summary').textContent = source.kind === 'arc' ?
      t('arcSummary', source.spaces, source.links) :
      t('bookmarksSummary', source.links);
  item.querySelector('.hint').textContent =
      t(source.kind === 'arc' ? 'arcHint' : 'bookmarksHint');
  button.textContent = t('importButton');
  button.setAttribute('aria-label',
      `${t('importButton')}: ${source.browser} ${source.profile}`.trim());

  button.addEventListener('click', async () => {
    button.disabled = true;
    button.textContent = t('working');
    result.hidden = true;
    error.hidden = true;
    try {
      const answer = await sendWithPromise('importSource', source.id);
      result.textContent = resultText(source, answer);
      result.hidden = false;
      button.textContent = t('importAgainButton');
    } catch (code) {
      error.textContent = t(ERRORS[code] || 'errorUnknown');
      error.hidden = false;
      button.textContent = t('importButton');
    } finally {
      button.disabled = false;
    }
  });
  return item;
}

async function load() {
  const sources = await sendWithPromise('getSources');
  $('searching').hidden = true;
  $('none').hidden = sources.length > 0;
  $('sources').hidden = sources.length === 0;
  $('source-list').replaceChildren(...sources.map(renderSource));
}

load();
)JS";

#endif  // CHROME_BROWSER_UI_WEBUI_ALERSE_IMPORT_ALERSE_IMPORT_PAGE_INC_
