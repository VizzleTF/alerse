# ADR-0002: Sync accounts: email, sync code and password, any server

Status: accepted, 2026-11-13. Steps 1 and 2 of the [order of work](#order-of-work)
are built (alerse-sync 0.2.0, Alerse 0.1.14): accounts with the 24 words and a
password, device keys, the device list, starting over from the old chains, and the
Alerse server at `sync.alerse.app`. [design.md, Sync](../design.md#sync) describes
what is built.

## Context

Sync today has no account. The first device makes a random 256-bit secret, shown as
24 words; every Space syncs as a chain whose keys derive from that secret
([ADR-0001](0001-space-profiles.md#sync)). Three ways bring a new device in: the
words, an optional login name and password that wrap the secret on the server
(`pw-v1`), and pairing with a short code (`pair-v1`). The user types the URL of their
own alerse-sync server.

What that leaves out:

- **The words alone open everything.** A photo of the recovery sheet is the whole
  account.
- **The login is not an email.** Nothing is verified, the first chain to take a name
  has it, and nothing tells the user that a new device joined.
- **Devices cannot be removed.** Every device signs its requests with the same chain
  key, so the server cannot refuse one of them, and the secret cannot be changed.
- **There is no server to pick.** Everyone has to run one.

The owner decided on 2026-11-13:

1. Two kinds of server, picked at setup: an official Alerse server and the user's own.
2. Two ways to sign in: an email address with a password, or the 24 words with a
   password. With the words, a new device needs both.
3. Real email: the server knows the address and sends codes to it.
4. The new scheme starts sync over. The data stays on the devices and is uploaded
   again; the old chains are removed from the server.
5. Open tabs, bookmarks and Spaces reach the other devices within seconds.

## Decision

### One account key, several locks

An account has one **account key**: 256 random bits, made on the first device and
never sent anywhere in the clear. Everything derives from it as the secret does
today: each Space's chain, its token keys and its sync passphrase (HKDF-SHA256 with
the labels of ADR-0001). Devices keep it encrypted by the OS (DPAPI, Keychain), as
today.

The server keeps the account key only inside **envelopes**, each opened by something
the user has:

| Envelope | Opened by | Server breach alone | Lost sheet alone |
|---|---|---|---|
| `email-v1` | email address + password | offline guessing of the password | — |
| `sk-v1` | 24 words + password | nothing: the words are 256 bits | nothing: the password is missing |
| `rk-v1` | 24 words (recovery code of an email account) | nothing | the account |
| `pair-v1` | a signed-in device, by short code | nothing | — |

An account has `email-v1` or `sk-v1`, never both. An email account also gets `rk-v1`
so that a forgotten password is not the end of it; its recovery sheet says the code
is the account. A words account has no recovery beyond its devices: forgetting the
password with no device signed in loses the data on the server, and the setup page
says so before it creates the account.

### Email and password (`email-v1`)

The derivation of `pw-v1` stays: scrypt (N = 2^17, r = 8, p = 1) of the password,
salted per account; HKDF splits it into an authentication key and a key that wraps
the account key. The server keeps the authentication key only as Argon2id, never
sees the password or the account key, and answers made-up parameters for unknown
addresses (no enumeration). Clients refuse KDF parameters below their own.

What email adds:

- **Verified address.** Creating an account sends a six-digit code, valid 10 minutes;
  the account exists once the code is entered. The first to verify an address has it.
- **A code on every new device.** Signing in on a device the account does not know
  takes the password and a code sent to the address. A stolen password alone does not
  sign in.
- **Notices.** A mail when a device joins, when the password or the address changes,
  and when a device is removed.
- **A forgotten password** is replaced from a signed-in device or with the recovery
  code. Email alone cannot recover the data: the server cannot open the envelope.
  With neither, "Delete the account" (after a code to the address) frees the address
  for a new account.

The address is stored as given, to send mail, plus a hash for lookups. That is the
price of real email: the server knows who the user is. It still never holds data it
can read.

OPAQUE (RFC 9807) would also keep the authentication key from the server and stop
offline guessing after a breach of the database alone. It needs hash-to-curve,
which Chromium's BoringSSL has only as an internal function. It stays a later scheme
(`email-v2`); every envelope names its scheme.

### 24 words and password (`sk-v1`)

The words are a **secret key** (as 1Password's Secret Key), not the account key
itself:

```text
sk      = the 256 bits the 24 words encode
id      = HKDF-SHA256(sk, info "alerse-sk-id")          # names the account to the server
pwkey   = scrypt(password, salt, N = 2^17, r = 8, p = 1)
master  = HKDF-SHA256(pwkey || HKDF-SHA256(sk, info "alerse-sk-mix"), info "alerse-sk-v1")
auth    = HKDF-SHA256(master, info "alerse-sk-auth")    # proves both, kept as Argon2id
wrap    = HKDF-SHA256(master, info "alerse-sk-wrap")    # opens the account key, never sent
```

A device that signs in sends `id` and `auth` and gets the envelope. Without the words
nobody can name the account or guess the password offline; without the password the
words open nothing, and the server limits online guesses as it does for logins. No
email is involved.

### Devices are known to the server

Today every device signs with the chain key, so the server cannot tell devices
apart. In the new scheme each device makes its own Ed25519 key pair, which joins the
account when the device opens an envelope or is paired. Requests are signed with the
device key; the server answers only keys of the account's active devices.

- **Remove a device**: any signed-in device, after the password (and, for email, a
  code), removes another. The server refuses its key from the next request on. Its
  copy of the data stays on it.
- **Change keys**: removal is enough against a lost laptop. If the account key itself
  may have leaked (a copied recovery sheet, a device that was compromised), "Change
  keys" makes a new account key, new chains for every Space, uploads the data again
  and deletes the old chains. Chromium cannot change a custom passphrase in place, so
  new chains are the only way.
- **The list** shows each device's name, platform and last contact, as Chromium's
  device info already syncs them.

The server now knows which chains belong to one account. It needs that to delete an
account, to remove a device from all its Spaces and for quotas; for an email account
it knows the user anyway. The chains' data stays end-to-end encrypted.

### Servers

The browser speaks to any alerse-sync server; the setup page asks first:

- **Alerse server** (official, run by the project; domain and operator to settle,
  see Open questions), the first choice.
- **My server**: a URL, as today; https only, plain http only to this computer.

Both run the same alerse-sync with the same protocol. A server without mail settings
offers only words and password (`sk-v1`); email accounts need SMTP. The official
server runs with `REQUIRE_CUSTOM_PASSPHRASE`, so its operator never holds readable
data.

### Changes reach the other devices in seconds

Sync is pushed, not polled. When a device commits, the server tells the account's
other devices at once over Server-Sent Events (`GET /v2/events`, already built), and
they fetch within 250 ms. Polling stays as the fallback, once an hour.

What bounds the delay is how long a device waits before it commits a local change.
Chromium's defaults, and what the server will send instead (`custom_nudge_delays` of
the protocol's `ClientCommand`, which Chromium applies as is):

| Data | Chromium waits | Alerse server sends |
|---|---|---|
| Bookmarks, pinned items, the list of Spaces (home's bookmarks) | 2 s | 2 s |
| Open tabs and windows | 15 s | 3 s |
| Passwords, settings | 0.2 s to 2 s | unchanged |
| History | 15 s | unchanged |

A tab therefore shows on the other device about 3 to 4 seconds after it opened, a
bookmark after 2 to 3. Three seconds still merges the burst of commits a single page
load makes (title, favicon, navigation). Polling every minute would be slower and
cost every device a request a minute; it is not used. The delays are server settings
(`SESSIONS_NUDGE_DELAY` and others), so they can be tuned without a browser release.

### Starting over

Clients of the new scheme do not join the old chains. On update, the Sync page says
sync has to be set up again: the user creates an account or signs in, every Space
uploads its data into the new chains, and the device clears its old chain on the
server (ClearServerData), which turns sync off on devices still on the old scheme.
The 24 words of an old chain mean nothing in the new scheme. The server keeps
serving `/v2` for one release so that old clients can still sync while devices are
updated, then drops it.

## Server changes (alerse-sync)

| Area | Change |
|---|---|
| Accounts | Tables `accounts` (id, kind, email and its hash, verified), `envelopes` (scheme, KDF parameters, salt, Argon2id of auth, wrapped key), `devices` (public key, name, created, last seen, removed), `account_chains` |
| API | `/v3/account` (create, verify, delete), `/v3/unlock` (params, unlock, new-device code), `/v3/devices` (list, remove), `/v3/keys` (change keys), `/v3/command/` and `/v3/events` signed by the device key, `/v3/pair` as `/v2/pair` |
| Mail | SMTP settings (`SMTP_URL`, `MAIL_FROM`); codes and notices in the user's language; per-address and per-account send limits |
| Sync speed | `custom_nudge_delays` in every response, from settings |
| Limits | Rate limits on codes and unlocks as today's login limits; Argon2id cost unchanged |
| Retention | Unverified accounts removed after a day; removed devices' records after 90 days |

## Browser changes

- `chrome://alerse-sync`: server choice, create or sign in (email or words), the
  code step, the device list with Remove, Change password, Change keys, the recovery
  sheet for both kinds, and the "set sync up again" notice after the update.
- The account key replaces today's secret in `Local State`; the per-Space derivation
  of ADR-0001 is unchanged.
- A device key per profile set, kept like the account key; tokens signed with it.
- iOS follows after the desktop platforms.

## Order of work

1. Server: accounts, envelopes `sk-v1`, device keys, `/v3` without mail; nudge delays
   (this one also helps today's clients).
2. Browser: the new page for "My server" with words and password, device list, start
   over from the old chains.
3. Server and browser: mail, `email-v1`, `rk-v1`, new-device codes, notices.
4. Official server: hosting, domain, mail provider, privacy policy; then the server
   choice on the page.
5. Change keys.
6. `email-v2` (OPAQUE), if hash-to-curve becomes usable.

## Consequences

- Stronger than today for words accounts: neither the sheet nor the server alone
  opens anything.
- Email accounts are as strong as their password against whoever holds the server's
  database; the new-device code protects against everyone else.
- The server learns the email address and which chains are one account; it still
  never reads data.
- Every user sets sync up once more after the update.

## Open questions

- **Official server**: domain, hosting, mail provider (with SPF, DKIM, DMARC),
  privacy policy and terms, who operates it. Owner's call; it costs money.
- **Quotas on the official server**: today's per-chain limits, or less.
- **Linking chains** for words accounts on a self-hosted server: needed for device
  removal; could stay unlinked by removing a device chain by chain. Proposed: linked.

## Links

- [ADR-0001: Space profiles](0001-space-profiles.md), sync of Spaces.
- [design.md, Sync](../design.md#sync), the scheme this replaces.
- [alerse-sync protocol notes](https://github.com/VizzleTF/alerse-sync/blob/main/docs/protocol.md).
