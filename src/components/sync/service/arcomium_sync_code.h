// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_ARCOMIUM_SYNC_CODE_H_
#define COMPONENTS_SYNC_SERVICE_ARCOMIUM_SYNC_CODE_H_

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace syncer {

// A sync chain's secret written as a sync code: 24 words of the BIP39 English
// list, the 256 bits of the secret followed by the first 8 bits of its
// SHA-256 as a checksum, 11 bits a word. The same encoding as a BIP39
// mnemonic, so the words are ones people know from wallets.
using SyncChainSecret = std::array<uint8_t, 32>;

// Returns a new random secret.
SyncChainSecret GenerateSyncChainSecret();

// Returns the 24 words of `secret`, separated by spaces.
std::string EncodeSyncCode(const SyncChainSecret& secret);

// Reads a sync code as people type it: any case, any whitespace between
// words, and each word may be cut to its first four letters, which tell
// every word apart. Returns nullopt for a wrong word, count or checksum.
std::optional<SyncChainSecret> DecodeSyncCode(std::string_view code);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_ARCOMIUM_SYNC_CODE_H_
