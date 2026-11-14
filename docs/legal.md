# Legal notes for publishing Alerse

What publishing Alerse builds involves legally, what each point needs and where it stands.
For maintainers; checked 2026-11-13. This is not legal advice.

| Topic | State | Where |
|---|---|---|
| Licenses of Chromium and its components | complied: every build ships their texts | chrome://credits |
| Alerse's and ungoogled-chromium's own licenses | complied: both listed in the credits | `branding/third_party/` |
| Publisher in file properties and Info.plist | The Alerse Authors, copyright shared with The Chromium Authors | `tools/rename_product.py` |
| Trademarks of others | disclaimer in NOTICE, README and the site; no Google or Arc logos | NOTICE |
| Artwork | our own logo and glyphs; no Apple or Arc files | below |
| Audio and video codecs | open risk: patent pools | below |
| Widevine | not shipped; WebKit plays DRM sites on macOS | below |
| Encryption export rules | exempt as public open source | below |
| Sync server data | privacy policy and terms published | alerse-sync `docs/privacy.md`, `docs/terms.md` |
| The alerse.app site | no cookies, nothing from third parties, logs at most 7 days | `site/`, its privacy page |
| Vulnerability reports | private reporting on in every repository | SECURITY.md |
| Donations | Buy Me a Coffee and Boosty, without promises in return | `.github/FUNDING.yml` |

## Licenses

Chromium is BSD 3-Clause; its third-party components carry BSD, MIT, Apache 2.0, LGPL
2.1 (ffmpeg), MPL 2.0 and other licenses. Binary redistribution must ship their notices,
which Chromium's credits page does (`tools/licenses/licenses.py`, built into every build).

- Alerse's own code and ungoogled-chromium's patches are BSD 3-Clause too. Their entries
  (`branding/third_party/alerse`, `branding/third_party/ungoogled-chromium`) are copied
  over the tree with the rest of `branding/`, so the credits list them.
- ffmpeg is LGPL 2.1, built without its GPL parts, and linked into the browser as
  Chromium does. LGPL 2.1 § 6 asks that users can relink it with a changed ffmpeg; the
  whole browser's source is public, so they can rebuild it.
- Each release names the source it was built from: the platform repository's tag, which
  pins this repository as a submodule. Keep release notes linking that tag.

## Trademarks

Alerse names Arc, Chrome, Chromium and other browsers only to say what it imports from or
is built on: a use the trademark owners' rules and nominative fair use allow. To keep it
that way:

- No logo, icon or product name of Google, The Browser Company or Atlassian in the
  browser, the installers or the site.
- "For people moving from Arc", "Arc-style": yes. "Arc 2", "the new Arc", Arc's logo: no.
- Arc's own feature names: Little Arc is "Little window" and Easels are not built. Peek
  and Boost are kept as the plain words they are (a peek at a link, a boost of a
  site), with no Arc branding.
- Screenshots for the site show neutral pages, not other companies' sites.

The name Alerse turned up no software trademark in a web search (2026-11-13). A search
in the EUIPO and USPTO registers would settle it before a registration of our own.

## Artwork

- The logo is Alerse's own (`artwork/`).
- Chromium's product logos are replaced in `branding/`. Google Chrome's own artwork is
  never built: Alerse builds the Chromium branding.
- The glyphs of the icon pickers, the address bar and the top bar are drawn in code from
  lines, circles and rounded rects, sized to match Arc's look. No Apple SF Symbols file,
  Arc asset catalog, font or sound is copied. SF Symbols' license forbids using them on
  other platforms, so a glyph must never be imported from them.
- Emoji come from the system's emoji font at runtime.

## Codecs

Release builds turn on `proprietary_codecs` with `ffmpeg_branding="Chrome"`, as
ungoogled-chromium does on Windows and macOS. ffmpeg then decodes H.264, HEVC and AAC,
which patent pools (Via LA, Access Advance) license for a fee per copy above free tiers.
Every Chromium fork that plays such video carries this; none has been asked to pay.

- Decide this deliberately: dropping the codecs breaks much of the web's video.
- On macOS and Windows, decoding through the system's codecs instead is the lower-risk
  path; it is not built.

## Widevine

`enable_widevine` is on, but the Widevine module is not in the builds, and Alerse cannot
download it: ungoogled-chromium removes the component updater. Widevine's license is
Google's to grant, so it must not be bundled.

Netflix and most paid streaming need it. On Windows and macOS the module also checks that
the browser carries a Verified Media Path (VMP) signature, which Google issues to
browsers it licenses (Brave, Vivaldi, Opera), directly or through castlabs' signing
service. The ways to it, each a decision of its own:

- Apply to Google for a Widevine license, then sign every release with VMP and let the
  browser fetch the module from Google, on the user's request, as Brave does.
- On macOS: play these sites in WebKit, Apple's engine, which has FairPlay. Alerse does
  this (see Protected video in `docs/design.md`). WebKit is the system's
  framework, linked and not shipped, so no license is needed.

Fetching Google's module without a license is not done: its terms are Google's, and
Netflix would still refuse it without VMP. On Windows Alerse offers Microsoft Edge for
these sites; on Linux the video does not play.

## Encryption export

Alerse uses standard cryptography (TLS, AES-GCM, Ed25519, scrypt). Its source is
public, so it falls under the public-availability exclusion of the US EAR (§ 734.3(b)(3))
and the general software note of the EU dual-use regulation. Nothing needs to be filed.

## Sync server data

The operator of sync.alerse.app is a data controller under the GDPR for the little it
holds: account and device records, device names, timestamps, and IP addresses in logs.
Data is end-to-end encrypted, so the operator cannot read synced content. The server's
[privacy policy](https://github.com/VizzleTF/alerse-sync/blob/main/docs/privacy.md) and
[terms](https://github.com/VizzleTF/alerse-sync/blob/main/docs/terms.md) cover it. Whoever
runs their own server is the controller of that one.

## The alerse.app site

Static pages served from the operator's own server: no cookies, no analytics, no fonts,
scripts or images from anyone else (its CSP allows only the site itself), so it needs no
consent banner. The web server keeps request logs at most 7 days, as its privacy page
says. It links the sync server's policy and terms.

## Open points

- A contact address for privacy requests: the policy points to GitHub for now.
- System codecs instead of bundled ones (see Codecs).
- Code signing: unsigned builds carry no legal risk, but signing ties a release to its
  publisher.
