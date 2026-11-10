// Copyright 2026 The Alerse Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/alerse_sync_login.h"

#include <vector>

#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

constexpr char kLogin[] = "alice@example.com";
constexpr char kPassword[] = "a long enough sync password";

std::vector<uint8_t> TestSalt() {
  std::vector<uint8_t> salt(32);
  for (size_t i = 0; i < salt.size(); ++i) {
    salt[i] = static_cast<uint8_t>(i);
  }
  return salt;
}

// Values computed independently with Go's x/crypto (scrypt, HKDF) and
// crypto/cipher (AES-GCM), following alerse-sync's docs/protocol.md.
TEST(AlerseSyncLoginTest, MatchesReferenceVector) {
  const std::string id = SyncLoginId(kLogin);
  EXPECT_EQ(id,
            "210f0db78bca37a6d88f3e4d9ede24b10fab03798f2e532fda54b1fb1568d3f3");
  const std::optional<SyncLoginKeys> keys =
      DeriveSyncLoginKeys(id, kPassword, TestSalt(), SyncLoginKdf());
  ASSERT_TRUE(keys);
  EXPECT_EQ(base::HexEncodeLower(keys->auth),
            "b109448183f13f3fd6b731394a43a0e47529a6a822970f1864bca35c7bef1735");
  EXPECT_EQ(base::HexEncodeLower(keys->enc),
            "16d3d3d9c5c46ed87b3dd0134c6a90f70f111f8dcc819d93401f7bcea52c2051");

  std::vector<uint8_t> wrapped;
  ASSERT_TRUE(base::HexStringToBytes(
      "01000000000000000000000000"
      "5b52de9abcefb659e28a580fe549c02060c277d079af39b0ec05d6e9e89045a4"
      "3a601c13d63f17a6fe4e699c69fb9b3f",
      &wrapped));
  SyncChainSecret all_ones;
  all_ones.fill(0xff);
  EXPECT_EQ(UnwrapSyncChainSecret(*keys, id, wrapped), all_ones);
}

TEST(AlerseSyncLoginTest, WrapRoundTrips) {
  const std::string id = SyncLoginId(kLogin);
  const SyncLoginKeys keys =
      *DeriveSyncLoginKeys(id, kPassword, TestSalt(), SyncLoginKdf());
  const SyncChainSecret secret = GenerateSyncChainSecret();
  const std::vector<uint8_t> wrapped = WrapSyncChainSecret(keys, id, secret);
  EXPECT_EQ(UnwrapSyncChainSecret(keys, id, wrapped), secret);
  // Another login's id, a changed byte or other keys do not open it.
  EXPECT_FALSE(UnwrapSyncChainSecret(keys, SyncLoginId("bob"), wrapped));
  std::vector<uint8_t> changed = wrapped;
  changed.back() ^= 1;
  EXPECT_FALSE(UnwrapSyncChainSecret(keys, id, changed));
  SyncLoginKeys other = keys;
  other.enc[0] ^= 1;
  EXPECT_FALSE(UnwrapSyncChainSecret(other, id, wrapped));
}

TEST(AlerseSyncLoginTest, NormalizesLogins) {
  EXPECT_EQ(NormalizeSyncLogin("  Alice@Example.COM \n"), kLogin);
  EXPECT_EQ(SyncLoginId(NormalizeSyncLogin("ALICE@example.com")),
            SyncLoginId(kLogin));
  EXPECT_EQ(NormalizeSyncLogin("   "), "");
  EXPECT_EQ(NormalizeSyncLogin(std::string(255, 'a')), "");
}

TEST(AlerseSyncLoginTest, RefusesWeakOrHugeParameters) {
  EXPECT_TRUE(IsAcceptableSyncLoginKdf(SyncLoginKdf()));
  EXPECT_TRUE(IsAcceptableSyncLoginKdf({.n = 1 << 18, .r = 8, .p = 1}));
  EXPECT_FALSE(IsAcceptableSyncLoginKdf({.n = 1 << 16, .r = 8, .p = 1}));
  EXPECT_FALSE(IsAcceptableSyncLoginKdf({.n = 1 << 17, .r = 4, .p = 1}));
  EXPECT_FALSE(IsAcceptableSyncLoginKdf({.n = 1 << 21, .r = 8, .p = 1}));
  EXPECT_FALSE(IsAcceptableSyncLoginKdf({.n = 3 << 16, .r = 8, .p = 1}));
  EXPECT_FALSE(IsAcceptableSyncLoginKdf({.n = 1 << 20, .r = 16, .p = 1}));
  EXPECT_FALSE(DeriveSyncLoginKeys(SyncLoginId(kLogin), kPassword, TestSalt(),
                                   {.n = 1024, .r = 8, .p = 1}));
}

TEST(AlerseSyncLoginTest, ChecksPasswords) {
  EXPECT_EQ(CheckSyncPassword("short password", kLogin),
            SyncPasswordProblem::kTooShort);
  EXPECT_EQ(CheckSyncPassword(std::string(257, 'x'), kLogin),
            SyncPasswordProblem::kTooLong);
  // Fifteen characters, not bytes.
  EXPECT_EQ(CheckSyncPassword("пароль-пароль!", kLogin),
            SyncPasswordProblem::kTooShort);
  EXPECT_EQ(CheckSyncPassword("123456789012345", kLogin),
            SyncPasswordProblem::kCommon);
  EXPECT_EQ(CheckSyncPassword("PasswordPassword", kLogin),
            SyncPasswordProblem::kCommon);
  EXPECT_EQ(CheckSyncPassword("aaaaaaaaaaaaaaaaaaaa", kLogin),
            SyncPasswordProblem::kCommon);
  EXPECT_EQ(CheckSyncPassword("abcdefghijklmnopq", kLogin),
            SyncPasswordProblem::kCommon);
  EXPECT_EQ(CheckSyncPassword("alice is my sync name", kLogin),
            SyncPasswordProblem::kContainsLogin);
  EXPECT_EQ(CheckSyncPassword("my arcomium password!", kLogin),
            SyncPasswordProblem::kContainsLogin);
  EXPECT_EQ(CheckSyncPassword("my Alerse password!!", kLogin),
            SyncPasswordProblem::kContainsLogin);
  // No rules about character classes: a long phrase is fine.
  EXPECT_EQ(CheckSyncPassword("purple tractor sings at dawn", kLogin),
            SyncPasswordProblem::kNone);
  EXPECT_EQ(CheckSyncPassword("пурпурный трактор поёт", kLogin),
            SyncPasswordProblem::kNone);
}

}  // namespace
}  // namespace syncer
