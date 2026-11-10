// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_ALERSE_SYNC_LOGIN_H_
#define COMPONENTS_SYNC_SERVICE_ALERSE_SYNC_LOGIN_H_

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "components/sync/service/alerse_sync_code.h"

namespace syncer {

// The optional sign-in of a sync chain: a login name (an email address or
// any name) and a password, from which a device derives two keys. The
// authentication key goes to the server, which hands the chain's secret,
// encrypted with the other key, to whoever proves it. The server never sees
// the login name, the password or the secret. Scheme "pw-v1" of
// alerse-sync's docs/protocol.md:
//
//   id     = hex(SHA-256("arcomium-sync-login-id-v1" | 0 | login))
//   master = scrypt(password, "arcomium-sync-login-v1" | 0 | id | salt)
//   auth   = HKDF-SHA256(master, "arcomium-sync-login-auth")
//   enc    = HKDF-SHA256(master, "arcomium-sync-login-enc")
//   wrapped secret = 1 | nonce | AES-256-GCM(enc, secret,
//                                            aad "arcomium-sync-login-v1" | id)
inline constexpr char kSyncLoginScheme[] = "pw-v1";

// NIST SP 800-63B-4: length is what makes a password strong; no rules on
// which characters it has.
inline constexpr size_t kMinSyncPasswordLength = 15;
inline constexpr size_t kMaxSyncPasswordLength = 256;

// scrypt parameters. The default costs 128 MiB and about half a second.
struct SyncLoginKdf {
  uint64_t n = 1 << 17;
  uint64_t r = 8;
  uint64_t p = 1;
  bool operator==(const SyncLoginKdf&) const = default;
};

struct SyncLoginKeys {
  std::array<uint8_t, 32> auth;
  std::array<uint8_t, 32> enc;
};

enum class SyncPasswordProblem {
  kNone,
  kTooShort,
  kTooLong,
  // On the list of common passwords, or one character or a run of
  // neighbouring ones repeated.
  kCommon,
  // Contains the login name, "alerse" or "arcomium" (the product's names).
  kContainsLogin,
};

// The login name as the server's id is derived from it: without surrounding
// whitespace, ASCII letters in lower case. Empty if nothing is left, it is
// longer than 254 bytes or not UTF-8.
std::string NormalizeSyncLogin(std::string_view login);

// The server's lookup key of a normalized login name: 64 lowercase hex
// digits.
std::string SyncLoginId(std::string_view normalized_login);

// Whether a server's parameters are ones this device derives keys with: at
// least a fixed minimum cost (the default of the first release), so a server
// cannot make it send an authentication key that is cheap to attack, and at
// most 1 GiB.
bool IsAcceptableSyncLoginKdf(const SyncLoginKdf& kdf);

// Length counts characters, not bytes.
SyncPasswordProblem CheckSyncPassword(std::string_view password,
                                      std::string_view normalized_login);

// A new random salt for SetLogin.
std::vector<uint8_t> NewSyncLoginSalt();

// Runs scrypt, which takes about half a second: call it off the UI thread.
// Returns nullopt if the parameters are not acceptable.
std::optional<SyncLoginKeys> DeriveSyncLoginKeys(std::string_view login_id,
                                                 std::string_view password,
                                                 base::span<const uint8_t> salt,
                                                 const SyncLoginKdf& kdf);

std::vector<uint8_t> WrapSyncChainSecret(const SyncLoginKeys& keys,
                                         std::string_view login_id,
                                         const SyncChainSecret& secret);

// Returns nullopt if `wrapped` was not made with these keys and id.
std::optional<SyncChainSecret> UnwrapSyncChainSecret(
    const SyncLoginKeys& keys,
    std::string_view login_id,
    base::span<const uint8_t> wrapped);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_ALERSE_SYNC_LOGIN_H_
