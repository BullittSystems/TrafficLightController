/*
 * test_wificodec.cpp
 *
 * Native unit tests for WifiCodec (/api/wifi JSON shapes; never exposes passwords).
 */
#include <gtest/gtest.h>

#include <ArduinoJson.h>

#include <cstring>
#include <string>

#include "WifiCodec.h"

using namespace WifiCodec;

namespace {
template <typename Fn>
void withObject(const char* jsonText, Fn fn) {
    JsonDocument doc;
    ASSERT_FALSE(deserializeJson(doc, jsonText));
    fn(doc.as<JsonObjectConst>());
}
}  // namespace

TEST(WifiCodecTest, WriteNetworksShape) {
    WifiSnapshot snap = {};
    snap.count        = 2;
    strcpy(snap.ssids[0], "Home");
    strcpy(snap.ssids[1], "Work");
    strcpy(snap.session, "Cafe");
    strcpy(snap.connected, "Cafe");

    JsonDocument doc;
    writeNetworks(doc, snap);
    ASSERT_EQ(doc["networks"].size(), 2u);
    EXPECT_STREQ(doc["networks"][0]["ssid"], "Home");
    EXPECT_STREQ(doc["networks"][1]["ssid"], "Work");
    EXPECT_STREQ(doc["session"], "Cafe");
    EXPECT_STREQ(doc["connected"], "Cafe");
    EXPECT_EQ(doc["max"].as<int>(), MAX_KNOWN_NETWORKS);

    std::string body;
    serializeJson(doc, body);
    EXPECT_EQ(body.find("password"), std::string::npos);
}

TEST(WifiCodecTest, WriteNetworksEmptyUsesNulls) {
    const WifiSnapshot snap = {};
    JsonDocument doc;
    writeNetworks(doc, snap);
    EXPECT_TRUE(doc["networks"].is<JsonArray>());
    EXPECT_EQ(doc["networks"].size(), 0u);
    EXPECT_TRUE(doc["session"].isNull());
    EXPECT_TRUE(doc["connected"].isNull());
}

TEST(WifiCodecTest, ParseAdd) {
    withObject(R"({"ssid":"Home","password":"secret123"})", [](JsonObjectConst obj) {
        const char* ssid = nullptr;
        const char* pass = nullptr;
        ASSERT_TRUE(parseAdd(obj, ssid, pass).ok);
        EXPECT_STREQ(ssid, "Home");
        EXPECT_STREQ(pass, "secret123");
    });
}

TEST(WifiCodecTest, ParseAddDefaultsToOpenNetwork) {
    withObject(R"({"ssid":"Open"})", [](JsonObjectConst obj) {
        const char* ssid = nullptr;
        const char* pass = nullptr;
        ASSERT_TRUE(parseAdd(obj, ssid, pass).ok);
        EXPECT_STREQ(pass, "");
    });
}

TEST(WifiCodecTest, ParseAddRejectsBadTypes) {
    const char* ssid = nullptr;
    const char* pass = nullptr;
    withObject(R"({})", [&](JsonObjectConst obj) { EXPECT_FALSE(parseAdd(obj, ssid, pass).ok); });
    withObject(R"({"ssid":5})",
               [&](JsonObjectConst obj) { EXPECT_FALSE(parseAdd(obj, ssid, pass).ok); });
    withObject(R"({"ssid":"Home","password":12345678})",
               [&](JsonObjectConst obj) { EXPECT_FALSE(parseAdd(obj, ssid, pass).ok); });
}

TEST(WifiCodecTest, ParseForget) {
    const char* ssid = nullptr;
    withObject(R"({"ssid":"Work"})", [&](JsonObjectConst obj) {
        ASSERT_TRUE(parseForget(obj, ssid).ok);
        EXPECT_STREQ(ssid, "Work");
    });
    withObject(R"({})", [&](JsonObjectConst obj) { EXPECT_FALSE(parseForget(obj, ssid).ok); });
}

TEST(WifiCodecTest, ParseKeep) {
    const char* replace = "sentinel";
    withObject(R"({})", [&](JsonObjectConst obj) {
        ASSERT_TRUE(parseKeep(obj, replace).ok);
        EXPECT_EQ(replace, nullptr);
    });
    withObject(R"({"replace":"Work"})", [&](JsonObjectConst obj) {
        ASSERT_TRUE(parseKeep(obj, replace).ok);
        EXPECT_STREQ(replace, "Work");
    });
    withObject(R"({"replace":1})",
               [&](JsonObjectConst obj) { EXPECT_FALSE(parseKeep(obj, replace).ok); });
}
