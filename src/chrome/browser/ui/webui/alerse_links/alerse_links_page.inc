// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The files of chrome://alerse-links, served by AlerseLinksUI.

#ifndef CHROME_BROWSER_UI_WEBUI_ALERSE_LINKS_ALERSE_LINKS_PAGE_INC_
#define CHROME_BROWSER_UI_WEBUI_ALERSE_LINKS_ALERSE_LINKS_PAGE_INC_

inline constexpr char kAlerseLinksHtml[] = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>Link routing</title>
<link rel="stylesheet" href="base.css">
<link rel="stylesheet" href="alerse_links.css">
<script type="module" src="alerse_links.js"></script>
</head>
<body>
<main>
  <header>
    <h1>Link routing</h1>
    <p class="lede">Choose the Space where links from other apps open, by
      site: links to github.com from a chat or mail app can open in your Work
      Space, signed in to work accounts. Links you open inside Alerse stay in
      the Space you are in.</p>
  </header>

  <form id="add" class="card" novalidate>
    <h2>Add a route</h2>
    <div class="fields">
      <div class="field">
        <label for="pattern">Site</label>
        <input id="pattern" placeholder="github.com or github.com/your-team"
            autocomplete="off" spellcheck="false">
      </div>
      <div class="field">
        <label for="space">Opens in</label>
        <select id="space"></select>
      </div>
    </div>
    <p id="add-error" class="error" role="alert" hidden></p>
    <button class="primary" type="submit">Add route</button>
  </form>

  <section class="card">
    <h2>Routes</h2>
    <p id="empty" class="note">No routes yet: links from other apps open in
      the Space you were in last.</p>
    <ul id="routes" class="routes"></ul>
  </section>
</main>
</body>
</html>
)HTML";

inline constexpr char kAlerseLinksCss[] = R"CSS(
.fields { display: grid; grid-template-columns: 2fr 1fr; gap: 12px; }
@media (max-width: 30rem) { .fields { grid-template-columns: 1fr; } }
.field { display: grid; gap: 6px; min-width: 0; }
.field label { margin: 0; }
select { padding: 7px 8px; }
.routes { list-style: none; margin: 0; padding: 0; display: grid; }
.routes li {
  display: grid;
  grid-template-columns: minmax(0, 1fr) auto auto;
  align-items: center;
  gap: 12px;
  padding: 8px 0;
  border-top: 1px solid var(--line);
}
.routes li:first-child { border-top: 0; }
.site { overflow-wrap: anywhere; font-weight: 500; }
.target { color: var(--muted); white-space: nowrap; }
.target::before { content: "\2192\00a0"; }
)CSS";

inline constexpr char kAlerseLinksJs[] = R"JS(
import {sendWithPromise} from 'chrome://resources/js/cr.js';

const $ = (id) => document.getElementById(id);

function render(state) {
  const select = $('space');
  const chosen = select.value;
  select.replaceChildren(...state.spaces.map((space) => new Option(space.name, space.id)));
  if (state.spaces.some((space) => space.id === chosen)) {
    select.value = chosen;
  } else if (state.spaces.length > 1) {
    select.value = state.spaces[1].id;  // the first Space after Home
  }

  $('empty').hidden = state.routes.length > 0;
  $('routes').replaceChildren(...state.routes.map((route) => {
    const li = document.createElement('li');
    const site = document.createElement('span');
    site.className = 'site';
    site.textContent = route.pattern;
    const target = document.createElement('span');
    target.className = 'target';
    target.textContent = route.name;
    const remove = document.createElement('button');
    remove.className = 'quiet';
    remove.textContent = 'Remove';
    remove.setAttribute('aria-label', `Remove the route for ${route.pattern}`);
    remove.addEventListener('click', async () => {
      render(await sendWithPromise('removeRoute', route.pattern));
    });
    li.append(site, target, remove);
    return li;
  }));
}

$('add').addEventListener('submit', async (e) => {
  e.preventDefault();
  $('add-error').hidden = true;
  try {
    render(await sendWithPromise('addRoute', $('pattern').value, $('space').value));
    $('pattern').value = '';
  } catch (message) {
    $('add-error').textContent = String(message);
    $('add-error').hidden = false;
  }
});

sendWithPromise('getState').then(render);
)JS";

#endif  // CHROME_BROWSER_UI_WEBUI_ALERSE_LINKS_ALERSE_LINKS_PAGE_INC_
