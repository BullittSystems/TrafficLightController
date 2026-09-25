/*
 * test_knownnetworks.cpp
 *
 * Native unit tests for KnownNetworks (saved Wi-Fi list: stable insertion order, no
 * eviction, in-place replace, validation, persistence).
 */
#include <gtest/gtest.h>

#include <string>

#include "KnownNetworks.h"
#include "Preferences.h"

using AddResult = KnownNetworks::AddResult;

class KnownNetworksTest : public ::testing::Test {
  protected:
    KnownNetworks nets;

    void SetUp() override { mock_prefs_clear(); }

    // Fill the list with net0..net(n-1), in that order.
    void fill(int n) {
        for (int i = 0; i < n; i++) {
            const std::string ssid = "net" + std::to_string(i);
            ASSERT_EQ(nets.add(ssid.c_str(), "password"), AddResult::Added);
        }
    }
};

TEST_F(KnownNetworksTest, StartsEmpty) {
    nets.load();
    EXPECT_EQ(nets.count(), 0);
}

TEST_F(KnownNetworksTest, AddAppendsInInsertionOrder) {
    EXPECT_EQ(nets.add("home", "homepass1"), AddResult::Added);
    EXPECT_EQ(nets.add("work", "workpass1"), AddResult::Added);
    ASSERT_EQ(nets.count(), 2);
    EXPECT_STREQ(nets.at(0).ssid, "home");
    EXPECT_STREQ(nets.at(1).ssid, "work");
}

TEST_F(KnownNetworksTest, ReAddingUpdatesPasswordInPlace) {
    nets.add("home", "oldpassword");
    nets.add("work", "workpass1");
    EXPECT_EQ(nets.add("home", "newpassword"), AddResult::Updated);
    ASSERT_EQ(nets.count(), 2);
    EXPECT_STREQ(nets.at(0).ssid, "home");  // position unchanged
    EXPECT_STREQ(nets.at(0).password, "newpassword");
    EXPECT_STREQ(nets.at(1).ssid, "work");
}

TEST_F(KnownNetworksTest, FullListRejectsAndNeverEvicts) {
    fill(MAX_KNOWN_NETWORKS);
    EXPECT_EQ(nets.add("extra", "password"), AddResult::Full);
    EXPECT_EQ(nets.count(), MAX_KNOWN_NETWORKS);
    EXPECT_EQ(nets.find("extra"), -1);
    EXPECT_EQ(nets.find("net0"), 0);  // nothing was dropped
}

TEST_F(KnownNetworksTest, FullListStillAcceptsPasswordUpdates) {
    fill(MAX_KNOWN_NETWORKS);
    EXPECT_EQ(nets.add("net3", "changedpass"), AddResult::Updated);
    EXPECT_STREQ(nets.at(3).password, "changedpass");
}

TEST_F(KnownNetworksTest, ReplaceSwapsInPlace) {
    fill(MAX_KNOWN_NETWORKS);
    EXPECT_EQ(nets.replace("net2", "extra", "extrapass"), AddResult::Added);
    EXPECT_EQ(nets.count(), MAX_KNOWN_NETWORKS);
    EXPECT_EQ(nets.find("net2"), -1);
    EXPECT_EQ(nets.find("extra"), 2);  // took net2's slot
    EXPECT_STREQ(nets.at(2).password, "extrapass");
    EXPECT_STREQ(nets.at(3).ssid, "net3");  // neighbors untouched
}

TEST_F(KnownNetworksTest, ReplaceUnknownTargetReturnsNotFound) {
    fill(2);
    EXPECT_EQ(nets.replace("nope", "extra", "extrapass"), AddResult::NotFound);
    EXPECT_EQ(nets.find("extra"), -1);
}

TEST_F(KnownNetworksTest, ReplaceWithInvalidCredentialsChangesNothing) {
    fill(2);
    EXPECT_EQ(nets.replace("net0", "extra", "short"), AddResult::Invalid);
    EXPECT_EQ(nets.find("net0"), 0);
}

TEST_F(KnownNetworksTest, ReplaceWithAlreadyKnownSsidJustUpdatesIt) {
    fill(3);
    EXPECT_EQ(nets.replace("net0", "net2", "newpass12"), AddResult::Updated);
    EXPECT_EQ(nets.find("net0"), 0);  // not displaced
    EXPECT_STREQ(nets.at(2).password, "newpass12");
}

TEST_F(KnownNetworksTest, ForgetRemovesAndCompacts) {
    fill(3);  // order: net0, net1, net2
    EXPECT_TRUE(nets.forget("net1"));
    ASSERT_EQ(nets.count(), 2);
    EXPECT_STREQ(nets.at(0).ssid, "net0");
    EXPECT_STREQ(nets.at(1).ssid, "net2");
    EXPECT_FALSE(nets.forget("nope"));
}

TEST_F(KnownNetworksTest, RejectsInvalidCredentials) {
    EXPECT_EQ(nets.add("", "password"), AddResult::Invalid);
    EXPECT_EQ(nets.add("0123456789012345678901234567890123", "password"),
              AddResult::Invalid);                             // 34-byte SSID
    EXPECT_EQ(nets.add("home", "short"), AddResult::Invalid);  // < 8
    EXPECT_EQ(nets.add("home", std::string(65, 'a').c_str()),  // > 64
              AddResult::Invalid);
    EXPECT_EQ(nets.add("home", std::string(64, 'z').c_str()),  // 64 non-hex
              AddResult::Invalid);
    EXPECT_EQ(nets.count(), 0);
}

TEST_F(KnownNetworksTest, AcceptsOpenNetworkAndHexPsk) {
    EXPECT_EQ(nets.add("cafe", ""), AddResult::Added);
    EXPECT_EQ(nets.add("office", std::string(64, 'a').c_str()), AddResult::Added);
}

TEST_F(KnownNetworksTest, PersistsAcrossReload) {
    nets.add("home", "homepass1");
    nets.add("work", "workpass1");

    KnownNetworks reloaded;
    reloaded.load();
    ASSERT_EQ(reloaded.count(), 2);
    EXPECT_STREQ(reloaded.at(0).ssid, "home");
    EXPECT_STREQ(reloaded.at(1).password, "workpass1");
}

TEST_F(KnownNetworksTest, ForgetDoesNotLeaveStalePasswordInStorage) {
    nets.add("home", "homepass1");
    nets.forget("home");
    // Slot 0 must be gone from NVS, not just hidden by the count.
    Preferences prefs;
    prefs.begin("tlwifi", true);
    EXPECT_FALSE(prefs.isKey("p0"));
}

TEST_F(KnownNetworksTest, ClearEmptiesAndPersists) {
    fill(3);
    nets.clear();
    EXPECT_EQ(nets.count(), 0);
    KnownNetworks reloaded;
    reloaded.load();
    EXPECT_EQ(reloaded.count(), 0);
}
