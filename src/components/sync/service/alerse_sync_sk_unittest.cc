// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/alerse_sync_sk.h"

#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

// The answers are the server's: alerse-sync derives them with Go's scrypt,
// HKDF and AES-GCM (its e2e/accounts_test.go is the reference client).
constexpr char kId[] =
    "d81ba5295b15e38adb783ffebcf80c427755f4a2c1e46efd5083a1c7f6a12882";
constexpr char kAuth[] =
    "4cec8e042c35e88b485aea9789ba4abfdc141e1c027a80f7ba1a1a89ed62473b";
constexpr char kWrapped[] =
    "01a0a1a2a3a4a5a6a7a8a9aaabd806c4ebc62de8af0936d0b53f49e4f735f9f3c988cf9"
    "abb07a88408f3e88227dbf64b884615b5ed165293d5260bc4a1";
// NFC, as the page sends it.
constexpr char kPassword[] = "Gr\xC3\xBCne \xC3\x84pfel unter dem Birnbaum";

SyncChainSecret Bytes(uint8_t first) {
  SyncChainSecret out;
  for (size_t i = 0; i < out.size(); ++i) {
    out[i] = static_cast<uint8_t>(first + i);
  }
  return out;
}

std::vector<uint8_t> Salt() {
  std::vector<uint8_t> salt(32);
  for (size_t i = 0; i < salt.size(); ++i) {
    salt[i] = static_cast<uint8_t>(32 + i);
  }
  return salt;
}

TEST(AlerseSyncSkTest, MatchesTheServer) {
  const SyncChainSecret sk = Bytes(0);
  EXPECT_EQ(SyncAccountId(sk), kId);
  std::optional<SyncAccountKeys> keys =
      DeriveSyncAccountKeys(sk, kPassword, Salt(), SyncKdf());
  ASSERT_TRUE(keys);
  EXPECT_EQ(keys->id, kId);
  EXPECT_EQ(base::HexEncodeLower(keys->auth), kAuth);

  std::vector<uint8_t> wrapped;
  ASSERT_TRUE(base::HexStringToBytes(kWrapped, &wrapped));
  std::optional<SyncChainSecret> account_key =
      UnwrapSyncAccountKey(*keys, wrapped);
  ASSERT_TRUE(account_key);
  SyncChainSecret expected;
  expected.fill(0x42);
  EXPECT_EQ(*account_key, expected);
}

TEST(AlerseSyncSkTest, WrapsAndRefusesOtherKeys) {
  const SyncChainSecret sk = Bytes(0);
  std::optional<SyncAccountKeys> keys =
      DeriveSyncAccountKeys(sk, kPassword, Salt(), SyncKdf());
  ASSERT_TRUE(keys);
  const SyncChainSecret account_key = Bytes(7);
  const std::vector<uint8_t> wrapped = WrapSyncAccountKey(*keys, account_key);
  EXPECT_EQ(UnwrapSyncAccountKey(*keys, wrapped), account_key);

  // Another password, or other words, opens nothing.
  std::optional<SyncAccountKeys> other = DeriveSyncAccountKeys(
      sk, "a different long password here", Salt(), SyncKdf());
  ASSERT_TRUE(other);
  EXPECT_FALSE(UnwrapSyncAccountKey(*other, wrapped));
  other = DeriveSyncAccountKeys(Bytes(1), kPassword, Salt(), SyncKdf());
  ASSERT_TRUE(other);
  EXPECT_FALSE(UnwrapSyncAccountKey(*other, wrapped));

  std::vector<uint8_t> tampered = wrapped;
  tampered.back() ^= 1;
  EXPECT_FALSE(UnwrapSyncAccountKey(*keys, tampered));
}

TEST(AlerseSyncSkTest, RefusesWeakParameters) {
  EXPECT_TRUE(IsAcceptableSyncKdf(SyncKdf()));
  EXPECT_FALSE(IsAcceptableSyncKdf({.n = 1 << 15, .r = 8, .p = 1}));
  EXPECT_FALSE(IsAcceptableSyncKdf({.n = 1 << 17, .r = 1, .p = 1}));
  EXPECT_FALSE(IsAcceptableSyncKdf({.n = (1 << 17) + 1, .r = 8, .p = 1}));
  EXPECT_FALSE(DeriveSyncAccountKeys(Bytes(0), kPassword, Salt(),
                                     {.n = 1 << 14, .r = 8, .p = 1}));
}

TEST(AlerseSyncSkTest, ChecksPasswords) {
  EXPECT_EQ(CheckSyncPassword("12345"), SyncPasswordProblem::kTooShort);
  // Six digits are enough: the words are the other secret.
  EXPECT_EQ(CheckSyncPassword("123456"), SyncPasswordProblem::kNone);
  EXPECT_EQ(CheckSyncPassword(std::string(257, 'x')),
            SyncPasswordProblem::kTooLong);
  EXPECT_EQ(CheckSyncPassword(kPassword), SyncPasswordProblem::kNone);
  // Characters, not bytes: five two-byte letters are too short.
  EXPECT_EQ(CheckSyncPassword("\xD1\x8F\xD0\xB1\xD0\xBB\xD0\xBE\xD0\xBA"),
            SyncPasswordProblem::kTooShort);
}

}  // namespace
}  // namespace syncer
