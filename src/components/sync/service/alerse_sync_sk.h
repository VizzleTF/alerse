// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_ALERSE_SYNC_SK_H_
#define COMPONENTS_SYNC_SERVICE_ALERSE_SYNC_SK_H_

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "components/sync/service/alerse_sync_code.h"

namespace syncer {

// A sync account opens with the 24 words and a password; a new device needs
// both (docs/adr/0002-sync-accounts.md). The words encode a 256-bit secret
// key, not the account key itself. Scheme "sk-v1" of alerse-sync's
// docs/protocol.md:
//
//   id      = hex(HKDF-SHA256(sk, info "alerse-sk-id"))      names the account
//   pwkey   = scrypt(password, salt, N, r, p)
//   master  = HKDF-SHA256(pwkey | HKDF-SHA256(sk, "alerse-sk-mix"), "alerse-sk-v1")
//   auth    = HKDF-SHA256(master, "alerse-sk-auth")          sent to the server
//   wrap    = HKDF-SHA256(master, "alerse-sk-wrap")          never leaves the device
//   wrapped = 1 | nonce | AES-256-GCM(wrap, account key, aad "alerse-sk-v1" | id)
//
// HKDF takes no salt. The password is UTF-8 in Unicode NFC; the caller
// normalizes it.
inline constexpr char kSyncAccountScheme[] = "sk-v1";

// NIST SP 800-63B-4: length is what makes a password strong; no rules on
// which characters it has.
inline constexpr size_t kMinSyncPasswordLength = 15;
inline constexpr size_t kMaxSyncPasswordLength = 256;

// scrypt parameters. The default costs 128 MiB and about half a second.
struct SyncKdf {
  uint64_t n = 1 << 17;
  uint64_t r = 8;
  uint64_t p = 1;
  bool operator==(const SyncKdf&) const = default;
};

struct SyncAccountKeys {
  std::string id;  // 64 lowercase hex digits
  std::array<uint8_t, 32> auth;
  std::array<uint8_t, 32> wrap;
};

enum class SyncPasswordProblem {
  kNone,
  kTooShort,
  kTooLong,
  // On the list of common passwords, or one character or a run of
  // neighbouring ones repeated.
  kCommon,
  // Contains "alerse" or "arcomium" (the product's names).
  kContainsName,
};

// The server's name of the account the words open: 64 lowercase hex digits.
std::string SyncAccountId(const SyncChainSecret& sk);

// Whether a server's parameters are ones this device derives keys with: at
// least the default cost, so a server cannot make it send an authentication
// key that is cheap to attack, and at most 1 GiB.
bool IsAcceptableSyncKdf(const SyncKdf& kdf);

// Length counts characters, not bytes.
SyncPasswordProblem CheckSyncPassword(std::string_view password);

// A new random salt for a new account.
std::vector<uint8_t> NewSyncSalt();

// Runs scrypt, which takes about half a second: call it off the UI thread.
// Returns nullopt if the parameters are not acceptable.
std::optional<SyncAccountKeys> DeriveSyncAccountKeys(
    const SyncChainSecret& sk,
    std::string_view password,
    base::span<const uint8_t> salt,
    const SyncKdf& kdf);

std::vector<uint8_t> WrapSyncAccountKey(const SyncAccountKeys& keys,
                                        const SyncChainSecret& account_key);

// Returns nullopt if `wrapped` was not made with these keys.
std::optional<SyncChainSecret> UnwrapSyncAccountKey(
    const SyncAccountKeys& keys,
    base::span<const uint8_t> wrapped);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_ALERSE_SYNC_SK_H_
