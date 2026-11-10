// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/alerse_sync_code.h"

#include <algorithm>
#include <vector>

#include "base/containers/span.h"
#include "base/rand_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace syncer {

namespace {

#include "components/sync/service/alerse_sync_words.inc"

constexpr size_t kWordCount = 24;
constexpr size_t kBitsPerWord = 11;
constexpr size_t kPrefixLength = 4;

uint8_t Checksum(const SyncChainSecret& secret) {
  uint8_t digest[SHA256_DIGEST_LENGTH];
  SHA256(secret.data(), secret.size(), digest);
  return digest[0];
}

// Returns the index of the word that `typed` names, or nullopt.
std::optional<size_t> FindWord(std::string_view typed) {
  const std::string_view prefix = typed.substr(0, kPrefixLength);
  const auto it = std::lower_bound(kSyncCodeWords.begin(),
                                   kSyncCodeWords.end(), prefix);
  if (it == kSyncCodeWords.end() || !it->starts_with(prefix)) {
    return std::nullopt;
  }
  // Four letters or more name one word, and the rest has to match it. Fewer
  // letters have to be a whole word, such as "add".
  const bool matches = typed.size() < kPrefixLength ? *it == typed
                                                    : it->starts_with(typed);
  if (!matches) {
    return std::nullopt;
  }
  return static_cast<size_t>(it - kSyncCodeWords.begin());
}

}  // namespace

SyncChainSecret GenerateSyncChainSecret() {
  SyncChainSecret secret;
  base::RandBytes(secret);
  return secret;
}

std::string EncodeSyncCode(const SyncChainSecret& secret) {
  // 264 bits: the secret, then the checksum byte.
  std::array<uint8_t, 33> bits;
  base::span(bits).first<32>().copy_from(secret);
  bits[32] = Checksum(secret);

  std::string code;
  for (size_t word = 0; word < kWordCount; ++word) {
    size_t index = 0;
    for (size_t bit = word * kBitsPerWord; bit < (word + 1) * kBitsPerWord;
         ++bit) {
      index = (index << 1) | ((bits[bit / 8] >> (7 - bit % 8)) & 1);
    }
    if (!code.empty()) {
      code += ' ';
    }
    code += kSyncCodeWords[index];
  }
  return code;
}

std::optional<SyncChainSecret> DecodeSyncCode(std::string_view code) {
  const std::string lower = base::ToLowerASCII(code);
  const std::vector<std::string_view> words = base::SplitStringPiece(
      lower, base::kWhitespaceASCII, base::TRIM_WHITESPACE,
      base::SPLIT_WANT_NONEMPTY);
  if (words.size() != kWordCount) {
    return std::nullopt;
  }

  std::array<uint8_t, 33> bits = {};
  size_t bit = 0;
  for (std::string_view typed : words) {
    const std::optional<size_t> index = FindWord(typed);
    if (!index) {
      return std::nullopt;
    }
    for (size_t i = kBitsPerWord; i-- > 0; ++bit) {
      if ((*index >> i) & 1) {
        bits[bit / 8] |= 1 << (7 - bit % 8);
      }
    }
  }

  SyncChainSecret secret;
  base::span(secret).copy_from(base::span(bits).first<32>());
  if (Checksum(secret) != bits[32]) {
    return std::nullopt;
  }
  return secret;
}

}  // namespace syncer
