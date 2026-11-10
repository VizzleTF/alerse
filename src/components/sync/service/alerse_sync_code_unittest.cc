// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/alerse_sync_code.h"

#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

SyncChainSecret FromHex(std::string_view hex) {
  SyncChainSecret secret;
  CHECK(base::HexStringToSpan(hex, secret));
  return secret;
}

// Test vectors of BIP39 (256-bit entropy).
TEST(AlerseSyncCodeTest, MatchesBip39Vectors) {
  EXPECT_EQ(EncodeSyncCode(FromHex(std::string(64, '0'))),
            "abandon abandon abandon abandon abandon abandon abandon abandon "
            "abandon abandon abandon abandon abandon abandon abandon abandon "
            "abandon abandon abandon abandon abandon abandon abandon art");
  EXPECT_EQ(EncodeSyncCode(FromHex(std::string(64, 'f'))),
            "zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo "
            "zoo zoo zoo zoo zoo zoo zoo vote");
}

TEST(AlerseSyncCodeTest, RoundTrips) {
  for (int i = 0; i < 100; ++i) {
    const SyncChainSecret secret = GenerateSyncChainSecret();
    EXPECT_EQ(DecodeSyncCode(EncodeSyncCode(secret)), secret);
  }
}

TEST(AlerseSyncCodeTest, ReadsCodesAsTyped) {
  const SyncChainSecret secret = FromHex(std::string(64, 'f'));
  EXPECT_EQ(DecodeSyncCode("  ZOO zoo\tzoo zoo zoo zoo zoo zoo zoo zoo zoo zoo\n"
                           "zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo zoo vote "),
            secret);
  // Four letters name a word.
  EXPECT_EQ(DecodeSyncCode("abandon abandon abandon abandon abandon abandon "
                           "aban aban aban aban aban aban aban aban aban aban "
                           "aban aban aban aban aban aban aban art"),
            FromHex(std::string(64, '0')));
}

TEST(AlerseSyncCodeTest, RejectsWrongCodes) {
  const std::string code = EncodeSyncCode(GenerateSyncChainSecret());
  // Too few words, an unknown word, a word cut too short, a bad checksum.
  EXPECT_FALSE(DecodeSyncCode(code.substr(0, code.rfind(' '))));
  EXPECT_FALSE(DecodeSyncCode("notaword " + code.substr(code.find(' ') + 1)));
  EXPECT_FALSE(DecodeSyncCode(
      "aba abandon abandon abandon abandon abandon abandon abandon abandon "
      "abandon abandon abandon abandon abandon abandon abandon abandon abandon "
      "abandon abandon abandon abandon abandon art"));
  EXPECT_FALSE(DecodeSyncCode(
      "abandon abandon abandon abandon abandon abandon abandon abandon abandon "
      "abandon abandon abandon abandon abandon abandon abandon abandon abandon "
      "abandon abandon abandon abandon abandon zoo"));
}

}  // namespace
}  // namespace syncer
