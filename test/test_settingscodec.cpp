/*
 * test_settingscodec.cpp
 *
 * Native unit tests for SettingsCodec (JSON parse/validate/serialize, clamping).
 */
#include <gtest/gtest.h>

#include <ArduinoJson.h>

#include "SettingsCodec.h"
#include "config.h"

using namespace SettingsCodec;

namespace {
// Parse a JSON literal and hand its root object to a callback.
template <typename Fn>
void withObject(const char* jsonText, Fn fn) {
    JsonDocument doc;
    ASSERT_FALSE(deserializeJson(doc, jsonText));
    fn(doc.as<JsonObjectConst>());
}
}  // namespace

TEST(SettingsCodecTest, ClampMsBounds) {
    EXPECT_EQ(clampMs(0), MIN_MS);
    EXPECT_EQ(clampMs(50), MIN_MS);
    EXPECT_EQ(clampMs(100), 100u);
    EXPECT_EQ(clampMs(600000), 600000u);
    EXPECT_EQ(clampMs(999999), MAX_MS);
}

TEST(SettingsCodecTest, NameRoundTrips) {
    EXPECT_STREQ(modeName(Mode::Api), "api");
    EXPECT_STREQ(modeName(Mode::Off), "off");
    EXPECT_STREQ(colorName(Lamp::Green), "green");
    EXPECT_STREQ(randomVariantName(RandomVariant::Multi), "multi");
    EXPECT_STREQ(apiKindName(ApiStateKind::Blink), "blink");
    EXPECT_STREQ(litName(0), "off");
    EXPECT_STREQ(litName(1u << static_cast<int>(Lamp::Red)), "red");
    EXPECT_STREQ(litName(0x07), "multi");
}

TEST(SettingsCodecTest, WriteSettingsShape) {
    AppSettings s;
    s.mode             = Mode::Blink;
    s.blink.color      = Lamp::Green;
    s.blink.intervalMs = 250;
    s.random.variant   = RandomVariant::Multi;

    JsonDocument doc;
    writeSettings(doc, s);

    EXPECT_STREQ(doc["mode"], "blink");
    EXPECT_EQ(doc["sequence"]["greenMs"].as<uint32_t>(), s.sequence.greenMs);
    EXPECT_STREQ(doc["random"]["variant"], "multi");
    EXPECT_STREQ(doc["blink"]["color"], "green");
    EXPECT_EQ(doc["blink"]["intervalMs"].as<uint32_t>(), 250u);
}

TEST(SettingsCodecTest, WriteStateInApiIncludesState) {
    ApiState api;
    api.kind  = ApiStateKind::Solid;
    api.color = Lamp::Red;
    JsonDocument doc;
    writeState(doc, Mode::Api, 1u << static_cast<int>(Lamp::Red), true, api);

    EXPECT_STREQ(doc["mode"], "api");
    EXPECT_STREQ(doc["lit"], "red");
    EXPECT_STREQ(doc["state"]["state"], "solid");
    EXPECT_STREQ(doc["state"]["color"], "red");
}

TEST(SettingsCodecTest, WriteStateOutsideApiHasNullState) {
    ApiState api;
    JsonDocument doc;
    writeState(doc, Mode::Sequence, 0, false, api);

    EXPECT_STREQ(doc["mode"], "sequence");
    EXPECT_STREQ(doc["lit"], "off");
    EXPECT_TRUE(doc["state"].isNull());
}

TEST(SettingsCodecTest, PatchUpdatesPresentFieldsOnly) {
    AppSettings s;  // defaults
    withObject(R"({"mode":"solid","solid":{"color":"yellow"}})", [&](JsonObjectConst obj) {
        Result r = applySettingsPatch(obj, s);
        EXPECT_TRUE(r.ok);
    });
    EXPECT_EQ(s.mode, Mode::Solid);
    EXPECT_EQ(s.solid.color, Lamp::Yellow);
    // untouched fields keep defaults
    EXPECT_EQ(s.sequence.greenMs, DEFAULT_GREEN_MS);
    EXPECT_EQ(s.blink.color, Lamp::Red);
}

TEST(SettingsCodecTest, PatchClampsDurations) {
    AppSettings s;
    withObject(R"({"sequence":{"greenMs":5,"redMs":9999999}})",
               [&](JsonObjectConst obj) { EXPECT_TRUE(applySettingsPatch(obj, s).ok); });
    EXPECT_EQ(s.sequence.greenMs, MIN_MS);
    EXPECT_EQ(s.sequence.redMs, MAX_MS);
}

TEST(SettingsCodecTest, PatchEnforcesRandomMaxGeMin) {
    AppSettings s;
    withObject(R"({"random":{"minMs":5000,"maxMs":1000}})",
               [&](JsonObjectConst obj) { EXPECT_TRUE(applySettingsPatch(obj, s).ok); });
    EXPECT_EQ(s.random.minMs, 5000u);
    EXPECT_EQ(s.random.maxMs, 5000u);  // bumped up to min
}

TEST(SettingsCodecTest, PatchRejectsUnknownMode) {
    AppSettings s;
    withObject(R"({"mode":"disco"})", [&](JsonObjectConst obj) {
        Result r = applySettingsPatch(obj, s);
        EXPECT_FALSE(r.ok);
        EXPECT_STREQ(r.error, "unknown mode");
    });
}

TEST(SettingsCodecTest, PatchRejectsInvalidColorAndVariant) {
    AppSettings s;
    withObject(R"({"solid":{"color":"purple"}})",
               [&](JsonObjectConst obj) { EXPECT_FALSE(applySettingsPatch(obj, s).ok); });
    withObject(R"({"random":{"variant":"chaos"}})",
               [&](JsonObjectConst obj) { EXPECT_FALSE(applySettingsPatch(obj, s).ok); });
}

TEST(SettingsCodecTest, ParseApiCommandVariants) {
    {
        ApiState st;
        st.intervalMs = 500;
        withObject(R"({"state":"off"})",
                   [&](JsonObjectConst obj) { EXPECT_TRUE(parseApiCommand(obj, st).ok); });
        EXPECT_EQ(st.kind, ApiStateKind::Off);
    }
    {
        ApiState st;
        st.intervalMs = 500;
        withObject(R"({"state":"blink","color":"yellow","intervalMs":250})",
                   [&](JsonObjectConst obj) { EXPECT_TRUE(parseApiCommand(obj, st).ok); });
        EXPECT_EQ(st.kind, ApiStateKind::Blink);
        EXPECT_EQ(st.color, Lamp::Yellow);
        EXPECT_EQ(st.intervalMs, 250u);
    }
}

TEST(SettingsCodecTest, ParseApiCommandKeepsCurrentIntervalWhenOmitted) {
    ApiState st;
    st.intervalMs = 777;
    withObject(R"({"state":"blink","color":"red"})",
               [&](JsonObjectConst obj) { EXPECT_TRUE(parseApiCommand(obj, st).ok); });
    EXPECT_EQ(st.intervalMs, 777u);
}

TEST(SettingsCodecTest, ParseApiCommandRejectsBadInput) {
    ApiState st;
    withObject(R"({"state":"nope"})", [&](JsonObjectConst obj) {
        Result r = parseApiCommand(obj, st);
        EXPECT_FALSE(r.ok);
        EXPECT_STREQ(r.error, "unknown state");
    });
    withObject(R"({"state":"solid"})", [&](JsonObjectConst obj) {
        Result r = parseApiCommand(obj, st);  // missing color
        EXPECT_FALSE(r.ok);
        EXPECT_STREQ(r.error, "invalid color");
    });
}
