// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_ALERSE_SYNC_PAIRING_H_
#define COMPONENTS_SYNC_SERVICE_ALERSE_SYNC_PAIRING_H_

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "components/sync/service/alerse_sync_code.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"

namespace syncer {

// Pairing a new device with one already in the chain: the existing device
// shows a one-time code, the user types it on the new device, and the two
// run SPAKE2 (BoringSSL) keyed by the code through the server, which only
// relays. With the agreed key the existing device sends the chain's secret,
// after the user allows the new device by name and checks that both show the
// same number. Scheme "pair-v1" of alerse-sync's docs/protocol.md:
//
//   code     = 12 characters of Crockford's base32: 4 for the slot, 8 (40
//              bits) for the password; shown as XXXX-XXXX-XXXX
//   slot     = hex(SHA-256("arcomium-pair-slot-v1" | 0 | slot part))
//   SPAKE2   password "arcomium-pair-v1" | 0 | slot | 0 | password part,
//            existing device Alice, new device Bob
//   keys     = HKDF-SHA256(SPAKE2 key, info "arcomium-pair-confirm" / "-box"
//              / "-verify")
//   confirm  = HMAC-SHA256(confirm key, "arcomium-pair-new-device" | 0 | name)
//   number   = 4 digits from the verify key, shown on both devices
//   box      = 1 | nonce | AES-256-GCM(box key, secret, aad
//              "arcomium-pair-v1" | slot)
struct SyncPairingCode {
  std::string slot_part;  // 4 characters
  std::string password;   // 8 characters

  // XXXX-XXXX-XXXX.
  std::string Display() const;
  // The server's slot id: 64 lowercase hex digits.
  std::string SlotId() const;
};

SyncPairingCode GenerateSyncPairingCode();

// Reads a code as people type it: any case, with or without dashes and
// spaces; O reads as 0, I and L as 1.
std::optional<SyncPairingCode> ParseSyncPairingCode(std::string_view typed);

class SyncPairing {
 public:
  enum class Role { kExisting, kNew };

  // Generates this side's SPAKE2 message.
  SyncPairing(Role role, const SyncPairingCode& code);
  SyncPairing(const SyncPairing&) = delete;
  SyncPairing& operator=(const SyncPairing&) = delete;
  ~SyncPairing();

  // 32 bytes for the other side.
  const std::vector<uint8_t>& message() const { return message_; }

  // Takes the other side's message. Returns false if it is malformed; a
  // wrong code shows only later, as a confirmation or box that does not
  // check out.
  bool Finish(base::span<const uint8_t> their_message);
  bool finished() const { return finished_; }

  // After Finish:
  std::string VerificationNumber() const;
  std::array<uint8_t, 32> Confirm(std::string_view device_name) const;
  bool CheckConfirm(std::string_view device_name,
                    base::span<const uint8_t> confirm) const;
  std::vector<uint8_t> Seal(const SyncChainSecret& secret) const;
  std::optional<SyncChainSecret> Open(base::span<const uint8_t> box) const;

 private:
  const std::string slot_;
  bssl::UniquePtr<SPAKE2_CTX> ctx_;
  std::vector<uint8_t> message_;
  bool finished_ = false;
  std::array<uint8_t, 32> confirm_key_{};
  std::array<uint8_t, 32> box_key_{};
  std::array<uint8_t, 32> verify_key_{};
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_ALERSE_SYNC_PAIRING_H_
