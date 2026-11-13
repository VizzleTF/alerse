"""Builds the alerse.app site into site/public: five static pages sharing one
header and footer. Run: python site/build.py"""
import html
import pathlib
import shutil

HERE = pathlib.Path(__file__).parent
OUT = HERE / 'public'
SYNC_DOCS = 'https://github.com/VizzleTF/alerse-sync/blob/main/docs'
VERSION = 'Alerse 0.1.13 · Chromium 157'

MOON = ('<svg class="icon-moon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" '
        'stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M12 3a6 6 0 0 0 9 9 9 9 0 1 1-9-9Z"/></svg>')
SUN = ('<svg class="icon-sun" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" '
       'stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><circle cx="12" cy="12" r="4"/>'
       '<path d="M12 2v2M12 20v2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M2 12h2M20 12h2M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4"/></svg>')
LOCK = ('<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" '
        'stroke-linejoin="round" aria-hidden="true"><rect x="4" y="11" width="16" height="10" rx="2"/>'
        '<path d="M8 11V7a4 4 0 0 1 8 0v4"/></svg>')


def page(name, title, description, body):
    nav = ''.join(
        f'<a href="{href}"{" aria-current=\"page\"" if key == name else ""}>{label}</a>'
        for key, href, label in (('features', '/#features', 'Features'), ('sync', '/sync', 'Sync'),
                                 ('github', 'https://github.com/VizzleTF/alerse', 'GitHub')))
    return f'''<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{html.escape(title)}</title>
<meta name="description" content="{html.escape(description)}">
<meta name="color-scheme" content="light dark">
<link rel="icon" href="/favicon.ico">
<link rel="stylesheet" href="/style.css">
<script src="/site.js"></script>
</head>
<body>
<a class="skip" href="#main">Skip to content</a>
<header class="header">
  <a class="brand" href="/"><img src="/images/logo.png" alt="" width="32" height="32">Alerse</a>
  <div class="header-right">
    <nav class="nav" aria-label="Main">{nav}</nav>
    <button class="toggle" type="button" aria-label="Switch theme">{MOON}{SUN}</button>
    <a class="button button-secondary button-small" href="/download">Download</a>
  </div>
</header>
<main id="main">
{body}
</main>
<footer class="footer">
  <div class="footer-main">
    <div class="footer-brand"><img src="/images/logo.png" alt="" width="24" height="24">
      <p>Alerse is a free, open-source browser built on ungoogled-chromium. Its name is a free spelling of alerce, the Chilean tree.</p></div>
    <nav class="footer-links" aria-label="Footer">
      <a href="https://github.com/VizzleTF/alerse">GitHub</a>
      <a href="https://github.com/VizzleTF/alerse-sync">Sync server</a>
      <a href="/privacy">Privacy</a>
      <a href="/terms">Terms</a>
      <a href="https://github.com/VizzleTF/alerse-win/releases">Releases</a>
      <a href="https://buymeacoffee.com/vizzletf">Buy me a coffee</a>
    </nav>
  </div>
  <p class="footer-legal">Not affiliated with Google, The Browser Company or Atlassian; Chrome and Chromium are trademarks of Google LLC, Arc of The Browser Company.</p>
</footer>
</body>
</html>
'''


def shot(file, alt, w, h, cls=''):
    return (f'<div class="shot {cls}"><img src="/images/{file}" alt="{html.escape(alt)}" '
            f'width="{w}" height="{h}" loading="lazy" decoding="async"></div>')


FEATURES = [
    ('Spaces', 'Each Space is a separate profile: its own cookies, logins, history and extensions, so you can be signed in to different accounts side by side. Switching is instant, and the Spaces look like one window.',
     ['Drag a tab onto a Space icon to move it', 'Link routing sends sites to their Space', 'Spaces survive restarts'],
     ('02-spaces-mac.png', 'The Work Space with its own pinned sites, opened from the Space bar.', 1600, 1000)),
    ('Command bar', 'Type an action instead of hunting for it: Copy URL, Copy URL as Markdown, Pin Tab, New Space, Edit Theme and more, each with its shortcut.',
     ['Search, history and open tabs in one field', 'Shortcuts shown next to every action'],
     ('03-command-bar-mac.png', 'The command bar finding Copy URL and Copy URL as Markdown.', 1600, 1000)),
    ('Split View', 'Two pages side by side in one tab, opened from a tab\'s menu or by dragging one tab onto another.',
     ['Each side keeps its own history'], ('04-split-view-mac.png', 'Two pages side by side in Split View.', 1600, 1000)),
    ('Icons for Spaces and folders', 'Arc\'s glyphs and emoji for Spaces, pinned items and folders, from the same picker.',
     ['Folders collapse or expand all at once'], ('05-icon-picker-mac.png', 'The icon picker of a Space.', 880, 760)),
    ('Windows, too', 'On Windows, Alerse looks like Arc on macOS everywhere except the top bar.',
     ['Windows 10 and 11, x64 and arm64'], ('06-windows.png', 'Alerse on Windows 11.', 1264, 792)),
]


def landing():
    features = ''.join(
        f'''<div class="feature{" flip" if i % 2 else ""}">
  <div class="feature-text"><h3>{t}</h3><p>{b}</p><ul>{"".join(f"<li>{x}</li>" for x in facts)}</ul></div>
  {shot(*img, cls="narrow" if img[0].startswith("05") else "")}
</div>''' for i, (t, b, facts, img) in enumerate(FEATURES))
    body = f'''<section class="hero">
  <div class="hero-text">
    <h1>A sidebar browser that keeps to itself.</h1>
    <p class="lead">Alerse gives you a vertical sidebar, Spaces with separate logins in one window, a command bar and end-to-end encrypted sync you can host yourself. Built on ungoogled-chromium. No account, no AI, no tracking.</p>
    <div class="cta">
      <a class="button button-primary" data-download href="/download">Download Alerse</a>
      <p class="small muted">{VERSION} · <a href="https://github.com/VizzleTF/alerse-win/releases">Release notes</a></p>
      <p class="small muted">Also for macOS (Apple Silicon) and Linux · <a href="/download">All platforms</a> · Free and open source</p>
    </div>
  </div>
  {shot("01-sidebar-mac.png", "Alerse on macOS: the Personal Space with pinned sites, a Reading folder and favorites in the sidebar.", 1600, 1000)}
</section>
<div class="rule"></div>
<section class="facts">
  <div><h3>Made for people leaving Arc.</h3><p>The sidebar, Spaces, the command bar and the icon pickers work the way Arc's do. Arc's Spaces, pinned tabs and folders import in one click.</p></div>
  <div><h3>Nothing to sign in to.</h3><p>No company account, no assistant, no AI search. Google is the default search engine only until you pick another.</p></div>
  <div><h3>Your data stays yours.</h3><p>ungoogled-chromium's privacy defaults. Sync is end-to-end encrypted; the server stores only data it cannot read.</p></div>
</section>
<section class="band" id="features"><div class="features">
  <div class="features-head"><h2>What you get</h2><p class="muted">The parts of Arc people kept using, rebuilt on Chromium you can inspect.</p></div>
  {features}
</div></section>
<section class="split">
  <div><h2>Sync that the server cannot read.</h2>
    <p>Bookmarks, open tabs, Spaces, passwords and settings sync between your devices. Everything is encrypted on the device before it leaves it. A new device signs in with 24 words and a password; neither alone opens the account. Best on a server of your own, in Docker or Kubernetes. If you would rather not run one, the Alerse server is offered voluntarily, without guarantee or responsibility for the data on it.</p>
    <p><a href="/sync">How sync works →</a></p></div>
  <dl class="kv">
    <div><dt>Speed</dt><dd>Tabs reach your other devices in about 15 seconds, other changes in about a minute.</dd></div>
    <div><dt>Devices</dt><dd>Each device has its own key and can be removed at any time.</dd></div>
    <div><dt>Server</dt><dd>Your own server is recommended: source code and a setup guide for Docker and Kubernetes on GitHub.</dd></div>
    <div><dt>Encryption</dt><dd>The 24 words never leave your devices. The server keeps data it cannot open.</dd></div>
  </dl>
</section>
<section class="band"><div class="split">
  <div><h2>Moving from Arc</h2><p class="muted">Chrome, Edge, Brave, Vivaldi, Opera and Yandex Browser bookmarks import the same way. Alerse only reads the other browser's files, so it can stay open.</p></div>
  <ol class="steps"><li>Open the Alerse menu: the logo at the top of the sidebar.</li><li>Choose Import from Another Browser.</li><li>Press Import next to Arc. Every Space arrives with its pinned tabs, folders and icons.</li></ol>
</div></section>
<section class="split">
  <div><h2>What is not there</h2><p class="muted">Said plainly, so you can decide before you download.</p></div>
  <ul class="gaps"><li>Not built: Easels, Boosts, Little Arc, Peek and the tab archive.</li><li>No Netflix and other DRM video yet: Google licenses Widevine only to browsers it has signed.</li><li>Builds are not code-signed by Apple or Microsoft yet: Windows SmartScreen asks once, macOS needs one terminal command.</li><li>Alerse tells you about updates; you install them yourself.</li><li>Linux waits for the next ungoogled-chromium release; iPhone is source only.</li></ul>
</section>
<section class="band"><div class="downloads">
  <h2>Download</h2>
  <div class="platforms">
    <div class="platform"><h3>Windows 10 and 11</h3><p class="small muted">x64 and arm64 · Installer or zip</p><p class="mono muted">Alerse 0.1.13 · Chromium 157</p><a class="button button-primary" href="https://github.com/VizzleTF/alerse-win/releases/latest">Download</a><a class="small" href="https://github.com/VizzleTF/alerse-win/releases">Release notes</a></div>
    <div class="platform"><h3>macOS</h3><p class="small muted">Apple Silicon · Disk image, pre-release</p><p class="mono muted">Alerse 0.1.13 · Chromium 156</p><a class="button button-secondary" href="https://github.com/VizzleTF/alerse-mac/releases">Download</a><a class="small" href="https://github.com/VizzleTF/alerse-mac/releases">Release notes</a></div>
    <div class="platform"><h3>Linux</h3><p class="small muted">x86_64 and arm64 · AppImage, older version</p><p class="mono muted">Alerse 0.1.1 · Chromium 156</p><a class="button button-secondary" href="https://github.com/VizzleTF/alerse-linux/releases/latest">Download</a><a class="small" href="https://github.com/VizzleTF/alerse-linux/releases">Release notes</a></div>
  </div>
  <p class="muted">Source code, issues and release notes are on <a href="https://github.com/VizzleTF/alerse">GitHub</a>.</p>
</div></section>'''
    return page('index', 'Alerse — a sidebar browser that keeps to itself',
                'A free, open-source sidebar browser for people leaving Arc: Spaces, a command bar and end-to-end encrypted sync, on ungoogled-chromium.', body)


def download():
    rows = [
        ('Windows 10 and 11', 'https://github.com/VizzleTF/alerse-win/releases/latest', True, 'x64, arm64', 'Installer, zip', '0.1.13 · Chromium 157',
         '<p class="small muted">SmartScreen warns about an unsigned app: More info, Run anyway.</p>'),
        ('macOS', 'https://github.com/VizzleTF/alerse-mac/releases', False, 'Apple Silicon', 'Disk image (.dmg), pre-release', '0.1.13 · Chromium 156',
         '<p class="small muted">Drag Alerse to Applications, then run once:</p><div class="code"><code>xattr -dr com.apple.quarantine /Applications/Alerse.app</code><button class="copy" type="button">Copy</button></div>'),
        ('Linux', 'https://github.com/VizzleTF/alerse-linux/releases/latest', False, 'x86_64, arm64', 'AppImage, tarball', '0.1.1 · Chromium 156',
         '<p class="small muted">Make the AppImage executable: chmod +x. An older version, before Spaces as profiles.</p>'),
        ('iPhone', 'https://github.com/VizzleTF/alerse-ios', None, '—', 'Source only', '—',
         '<p class="small muted">Build from the alerse-ios repository. Sync works; Spaces and the sidebar are not on iOS.</p>'),
    ]
    body_rows = ''.join(
        f'''<div class="row"><div class="name">{name}{f'<a class="button button-small {"button-primary" if main else "button-secondary"}" href="{href}">Download</a>' if main is not None else f'<a class="small" href="{href}">View source</a>'}</div>
<div>{arch}</div><div>{files}</div><div class="mono muted">{ver}</div><div>{notes}</div></div>''' for name, href, main, arch, files, ver, notes in rows)
    body = f'''<div class="page">
  <div class="page-head"><h1>Download Alerse</h1><p class="muted">Free and open source. Pick your platform; the notes under each say what happens on first start.</p></div>
  <div class="table"><div class="head"><span>Platform</span><span>Architectures</span><span>Files</span><span>Version</span><span>First start</span></div>{body_rows}</div>
  <div class="three">
    <div><h3>Updates</h3><p>Alerse checks GitHub once a day and shows "Update to Alerse X.Y.Z" in the Alerse menu. Nothing downloads by itself.</p></div>
    <div><h3>Verify</h3><p>Every release lists SHA-256 sums. The sync server's images are signed with cosign.</p></div>
    <div><h3>From Arc</h3><p>After the first start, open the Alerse menu and choose Import from Another Browser.</p></div>
  </div>
</div>'''
    return page('download', 'Download Alerse', 'Download Alerse for Windows, macOS or Linux.', body)


def sync():
    body = f'''<article class="reading">
  <h1>How sync works</h1>
  <p class="muted">Alerse syncs your browser between your devices through a server that stores only data it cannot read. This page says what syncs, how the encryption works and what to do when a device is lost.</p>
  <div class="diagram" role="img" aria-label="Your devices hold the keys; the sync server between them sees only ciphertext.">
    <div class="node">Your laptop<span>has the keys</span></div><div class="link">{LOCK}encrypted</div>
    <div class="node">Sync server<span>sees only ciphertext</span></div><div class="link">{LOCK}encrypted</div>
    <div class="node">Your other device<span>has the keys</span></div>
  </div>
  <h2>What syncs</h2>
  <p>Bookmarks, open tabs, Spaces, passwords, history and settings. Each Space syncs on its own, so a work Space stays apart from a personal one. The Sync page lists every Space with its tabs on this device and on each of the others.</p>
  <h2>How the encryption works</h2>
  <p>When you create an account, Alerse makes 24 words. They hold the account's secret key and never leave your devices. With your password they derive the keys that encrypt everything before it is sent.</p>
  <ul><li>A new device needs both the 24 words and the password; neither alone opens the account.</li><li>The server cannot reset either. Keep the words on paper or in a password manager, apart from the password.</li><li>Each device signs its requests with a key of its own.</li></ul>
  <h2>Your own server, or the Alerse server</h2>
  <p>A server of your own is the better choice: your data stays with you. Use the alerse-sync image with Postgres, in Docker or Kubernetes; the <a href="https://github.com/VizzleTF/alerse-sync#run-your-own-server">guide</a> is in the alerse-sync repository.</p>
  <pre class="codeblock"><code>docker run -e DATABASE_URL=postgres://… -p 8295:8295 ghcr.io/vizzletf/alerse-sync:0.2.1</code></pre>
  <p>If you would rather not run one, pick the Alerse server at sync.alerse.app. Its operator offers it voluntarily and free, without any guarantee, and takes no responsibility for the data on it; it may stop, and it holds a few accounts for now. <a href="/terms">Terms</a> · <a href="/privacy">Privacy</a></p>
  <h2>If you lose a device</h2>
  <p>Open Sync on another device, choose Devices and remove the lost one with your password. It stops getting anything new from its next request on. Its own copy of the data stays on it, so lock or wipe it as well.</p>
  <h2>Limits</h2>
  <p>Devices send only what changed, in batches: open tabs about 15 seconds after a change, everything else about a minute after. A device with nothing new sends nothing but an hourly check.</p>
  <ul><li>The server enforces its own limits per device and per Space, whatever a client sends.</li><li>An address that keeps sending refused requests is shut out for 15 minutes.</li></ul>
</article>'''
    return page('sync', 'How Alerse sync works', 'End-to-end encrypted sync for Alerse: on your own server or the Alerse server.', body)


def privacy():
    body = f'''<article class="reading">
  <h1>Privacy</h1>
  <p class="muted">What this site and the Alerse sync server keep about you.</p>
  <h2>This site</h2>
  <ul><li>No cookies, no analytics, no advertising, no fonts, scripts or images from other companies.</li><li>Your theme choice stays in your browser (local storage) and is never sent.</li><li>The web server logs each request's IP address, time, page and status to keep the site running and to stop abuse, and deletes them after 7 days.</li></ul>
  <h2>The browser</h2>
  <p>What Alerse sends on its own, such as a daily check for updates on GitHub, is listed in the <a href="https://github.com/VizzleTF/alerse#privacy">README</a>.</p>
  <h2>The sync server</h2>
  <p>sync.alerse.app has its own <a href="{SYNC_DOCS}/privacy.md">privacy policy</a>: what it keeps, why, for how long, and how to have it deleted.</p>
  <h2>Contact</h2>
  <p>The site and the sync server are run by Alerse's maintainer, VizzleTF on GitHub. Privacy requests: <a href="https://github.com/VizzleTF/alerse-sync/security/advisories/new">a private report</a> with "privacy" in its title.</p>
</article>'''
    return page('privacy', 'Privacy — Alerse', 'What alerse.app and the Alerse sync server keep about you.', body)


def terms():
    body = f'''<article class="reading">
  <h1>Terms</h1>
  <ul><li>Alerse is free software under the <a href="https://github.com/VizzleTF/alerse/blob/main/LICENSE">BSD 3-Clause license</a>, provided as it is, without warranty of any kind. Its components' licenses are listed at chrome://credits in the browser.</li><li>The Alerse sync server has its own <a href="{SYNC_DOCS}/terms.md">terms of use</a>: it is offered voluntarily, without guarantee or responsibility for the data on it.</li><li>Alerse is not affiliated with Google, The Browser Company or Atlassian. Chrome and Chromium are trademarks of Google LLC, Arc of The Browser Company.</li></ul>
</article>'''
    return page('terms', 'Terms — Alerse', 'Licence and terms of Alerse and its sync server.', body)


def main():
    if OUT.exists():
        shutil.rmtree(OUT)
    (OUT / 'images').mkdir(parents=True)
    for f in (HERE / 'images').iterdir():
        shutil.copy(f, OUT / 'images' / f.name)
    for f in ('style.css', 'site.js', 'favicon.ico', 'robots.txt'):
        shutil.copy(HERE / f, OUT / f)
    pages = {'index': landing, 'download': download, 'sync': sync, 'privacy': privacy, 'terms': terms}
    for name, make in pages.items():
        (OUT / f'{name}.html').write_text(make(), encoding='utf-8', newline='\n')
    print('built', ', '.join(pages), 'into', OUT)


if __name__ == '__main__':
    main()
