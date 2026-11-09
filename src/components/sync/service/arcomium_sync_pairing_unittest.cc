// Copyright 2026 The Arcomium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/arcomium_sync_pairing.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

TEST(ArcomiumSyncPairingTest, CodesRoundTrip) {
  for (int i = 0; i < 50; ++i) {
    const SyncPairingCode code = GenerateSyncPairingCode();
    const std::string shown = code.Display();
    ASSERT_EQ(shown.size(), 14u);
    const std::optional<SyncPairingCode> parsed = ParseSyncPairingCode(shown);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->SlotId(), code.SlotId());
    EXPECT_EQ(parsed->password, code.password);
  }
}

TEST(ArcomiumSyncPairingTest, ReadsCodesAsTyped) {
  const std::optional<SyncPairingCode> code =
      ParseSyncPairingCode(" ab0c 1dEf-ghjk ");
  ASSERT_TRUE(code);
  EXPECT_EQ(code->Display(), "AB0C-1DEF-GHJK");
  // O, I and L are read as the digits they look like.
  EXPECT_EQ(ParseSyncPairingCode("abOc-IdEf-ghjk")->Display(),
            "AB0C-1DEF-GHJK");
  EXPECT_FALSE(ParseSyncPairingCode("AB0C-1DEF-GHJ"));
  EXPECT_FALSE(ParseSyncPairingCode("AB0C-1DEF-GHJU"));
}

TEST(ArcomiumSyncPairingTest, PairsWithTheSameCode) {
  const SyncPairingCode code = GenerateSyncPairingCode();
  SyncPairing existing(SyncPairing::Role::kExisting, code);
  SyncPairing added(SyncPairing::Role::kNew, *ParseSyncPairingCode(
                                                 code.Display()));
  ASSERT_TRUE(added.Finish(existing.message()));
  ASSERT_TRUE(existing.Finish(added.message()));
  EXPECT_EQ(existing.VerificationNumber(), added.VerificationNumber());
  EXPECT_EQ(existing.VerificationNumber().size(), 4u);

  const auto confirm = added.Confirm("Laptop");
  EXPECT_TRUE(existing.CheckConfirm("Laptop", confirm));
  EXPECT_FALSE(existing.CheckConfirm("Phone", confirm));

  const SyncChainSecret secret = GenerateSyncChainSecret();
  EXPECT_EQ(added.Open(existing.Seal(secret)), secret);
}

TEST(ArcomiumSyncPairingTest, WrongCodeFails) {
  const SyncPairingCode code = GenerateSyncPairingCode();
  SyncPairingCode wrong = code;
  wrong.password[0] = wrong.password[0] == 'A' ? 'B' : 'A';
  SyncPairing existing(SyncPairing::Role::kExisting, code);
  SyncPairing added(SyncPairing::Role::kNew, wrong);
  ASSERT_TRUE(added.Finish(existing.message()));
  ASSERT_TRUE(existing.Finish(added.message()));
  EXPECT_FALSE(existing.CheckConfirm("Laptop", added.Confirm("Laptop")));
  EXPECT_FALSE(added.Open(existing.Seal(GenerateSyncChainSecret())));
}

}  // namespace
}  // namespace syncer
