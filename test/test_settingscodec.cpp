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
    EXPECT_STREQ(modeName(Mode::Off), "off");
    EXPECT_STREQ(modeName(Mode::Chase), "chase");
    EXPECT_STREQ(colorName(Lamp::Green), "green");
    EXPECT_STREQ(randomVariantName(RandomVariant::Multi), "multi");
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
    EXPECT_EQ(doc["chase"]["intervalMs"].as<uint32_t>(), s.chase.intervalMs);
}

TEST(SettingsCodecTest, PatchUpdatesChaseInterval) {
    AppSettings s;
    withObject(R"({"mode":"chase","chase":{"intervalMs":250}})",
               [&](JsonObjectConst obj) { EXPECT_TRUE(applySettingsPatch(obj, s).ok); });
    EXPECT_EQ(s.mode, Mode::Chase);
    EXPECT_EQ(s.chase.intervalMs, 250u);
}

TEST(SettingsCodecTest, WriteStateWithoutOverride) {
    LiveStatus st;
    st.mode = Mode::Sequence;
    JsonDocument doc;
    writeState(doc, st);

    EXPECT_STREQ(doc["mode"], "sequence");
    EXPECT_STREQ(doc["active"], "sequence");
    EXPECT_STREQ(doc["lit"], "off");
    EXPECT_TRUE(doc["override"].isNull());
}

TEST(SettingsCodecTest, WriteStateWithTimedOverride) {
    LiveStatus st;
    st.mode                             = Mode::Chase;
    st.litMask                          = 1u << static_cast<int>(Lamp::Red);
    st.overridden                       = true;
    st.active.settings.mode             = Mode::Blink;
    st.active.settings.blink.color      = Lamp::Red;
    st.active.settings.blink.intervalMs = 200;
    st.active.ttlMs                     = 60000;
    st.remainingMs                      = 42000;
    JsonDocument doc;
    writeState(doc, st);

    EXPECT_STREQ(doc["mode"], "chase");
    EXPECT_STREQ(doc["active"], "blink");
    EXPECT_STREQ(doc["lit"], "red");
    EXPECT_STREQ(doc["override"]["pattern"], "blink");
    EXPECT_STREQ(doc["override"]["color"], "red");
    EXPECT_EQ(doc["override"]["intervalMs"].as<uint32_t>(), 200u);
    EXPECT_EQ(doc["override"]["ttlMs"].as<uint32_t>(), 60000u);
    EXPECT_EQ(doc["override"]["remainingMs"].as<uint32_t>(), 42000u);
    EXPECT_TRUE(doc["override"]["greenMs"].isNull());  // other patterns' params omitted
}

TEST(SettingsCodecTest, WriteStateUntimedOverrideOmitsTtl) {
    LiveStatus st;
    st.overridden           = true;
    st.active.settings.mode = Mode::Off;
    JsonDocument doc;
    writeState(doc, st);

    EXPECT_STREQ(doc["override"]["pattern"], "off");
    EXPECT_TRUE(doc["override"]["ttlMs"].isNull());
    EXPECT_TRUE(doc["override"]["remainingMs"].isNull());
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

TEST(SettingsCodecTest, ApiIsNoLongerAMode) {
    Mode m = Mode::Solid;
    withObject(R"({"m":"api"})",
               [&](JsonObjectConst obj) { EXPECT_FALSE(parseMode(obj["m"], m)); });
}

TEST(SettingsCodecTest, ParseOverrideUsesGivenParams) {
    AppSettings saved;
    Override ov;
    withObject(R"({"pattern":"blink","color":"yellow","intervalMs":250,"ttlMs":60000})",
               [&](JsonObjectConst obj) { EXPECT_TRUE(parseOverride(obj, saved, ov).ok); });
    EXPECT_EQ(ov.settings.mode, Mode::Blink);
    EXPECT_EQ(ov.settings.blink.color, Lamp::Yellow);
    EXPECT_EQ(ov.settings.blink.intervalMs, 250u);
    EXPECT_EQ(ov.ttlMs, 60000u);
}

TEST(SettingsCodecTest, ParseOverrideFallsBackToSavedParams) {
    AppSettings saved;
    saved.blink.color      = Lamp::Green;
    saved.blink.intervalMs = 777;
    Override ov;
    withObject(R"({"pattern":"blink"})",
               [&](JsonObjectConst obj) { EXPECT_TRUE(parseOverride(obj, saved, ov).ok); });
    EXPECT_EQ(ov.settings.blink.color, Lamp::Green);
    EXPECT_EQ(ov.settings.blink.intervalMs, 777u);
    EXPECT_EQ(ov.ttlMs, 0u);  // no expiry
}

TEST(SettingsCodecTest, ParseOverrideEveryPattern) {
    AppSettings saved;
    Override ov;
    withObject(R"({"pattern":"sequence","greenMs":1000,"yellowMs":500,"redMs":2000})",
               [&](JsonObjectConst obj) { ASSERT_TRUE(parseOverride(obj, saved, ov).ok); });
    EXPECT_EQ(ov.settings.sequence.yellowMs, 500u);
    withObject(R"({"pattern":"random","variant":"multi","minMs":900,"maxMs":300})",
               [&](JsonObjectConst obj) { ASSERT_TRUE(parseOverride(obj, saved, ov).ok); });
    EXPECT_EQ(ov.settings.random.variant, RandomVariant::Multi);
    EXPECT_EQ(ov.settings.random.maxMs, 900u);  // bumped up to min
    withObject(R"({"pattern":"chase","intervalMs":150})",
               [&](JsonObjectConst obj) { ASSERT_TRUE(parseOverride(obj, saved, ov).ok); });
    EXPECT_EQ(ov.settings.chase.intervalMs, 150u);
    withObject(R"({"pattern":"off"})",
               [&](JsonObjectConst obj) { ASSERT_TRUE(parseOverride(obj, saved, ov).ok); });
    EXPECT_EQ(ov.settings.mode, Mode::Off);
}

TEST(SettingsCodecTest, ParseOverrideClampsTtl) {
    AppSettings saved;
    Override ov;
    withObject(R"({"pattern":"off","ttlMs":5})",
               [&](JsonObjectConst obj) { ASSERT_TRUE(parseOverride(obj, saved, ov).ok); });
    EXPECT_EQ(ov.ttlMs, MIN_TTL_MS);
    withObject(R"({"pattern":"off","ttlMs":999999999})",
               [&](JsonObjectConst obj) { ASSERT_TRUE(parseOverride(obj, saved, ov).ok); });
    EXPECT_EQ(ov.ttlMs, MAX_TTL_MS);
}

TEST(SettingsCodecTest, ParseOverrideRejectsBadInput) {
    AppSettings saved;
    Override ov;
    withObject(R"({"pattern":"disco"})", [&](JsonObjectConst obj) {
        Result r = parseOverride(obj, saved, ov);
        EXPECT_FALSE(r.ok);
        EXPECT_STREQ(r.error, "unknown pattern");
    });
    withObject(R"({})",
               [&](JsonObjectConst obj) { EXPECT_FALSE(parseOverride(obj, saved, ov).ok); });
    withObject(R"({"pattern":"solid","color":"purple"})",
               [&](JsonObjectConst obj) { EXPECT_FALSE(parseOverride(obj, saved, ov).ok); });
    withObject(R"({"pattern":"off","ttlMs":-5})",
               [&](JsonObjectConst obj) { EXPECT_FALSE(parseOverride(obj, saved, ov).ok); });
    withObject(R"({"pattern":"off","ttlMs":"soon"})",
               [&](JsonObjectConst obj) { EXPECT_FALSE(parseOverride(obj, saved, ov).ok); });
}

TEST(SettingsCodecTest, ParseOverrideRejectsForeignParams) {
    AppSettings saved;
    Override ov;
    withObject(R"({"pattern":"blink","greenMs":1000})", [&](JsonObjectConst obj) {
        Result r = parseOverride(obj, saved, ov);
        EXPECT_FALSE(r.ok);
        EXPECT_STREQ(r.error, "parameter not valid for this pattern");
    });
    withObject(R"({"pattern":"off","color":"red"})",
               [&](JsonObjectConst obj) { EXPECT_FALSE(parseOverride(obj, saved, ov).ok); });
    withObject(R"({"pattern":"solid","bogus":1})",
               [&](JsonObjectConst obj) { EXPECT_FALSE(parseOverride(obj, saved, ov).ok); });
    withObject(R"({"pattern":"off","ttlMs":0})",
               [&](JsonObjectConst obj) { EXPECT_FALSE(parseOverride(obj, saved, ov).ok); });
}

TEST(SettingsCodecTest, ParseOverrideRejectsNonNumericDurations) {
    AppSettings saved;
    Override ov;
    withObject(R"({"pattern":"blink","intervalMs":"fast"})", [&](JsonObjectConst obj) {
        Result r = parseOverride(obj, saved, ov);
        EXPECT_FALSE(r.ok);
        EXPECT_STREQ(r.error, "durations must be non-negative integers");
    });
    withObject(R"({"pattern":"chase","intervalMs":-1})",
               [&](JsonObjectConst obj) { EXPECT_FALSE(parseOverride(obj, saved, ov).ok); });
}

TEST(SettingsCodecTest, ParseModeCommand) {
    AppSettings saved;
    AppSettings out;
    withObject(R"({"pattern":"blink","color":"green","intervalMs":300})",
               [&](JsonObjectConst obj) { ASSERT_TRUE(parseModeCommand(obj, saved, out).ok); });
    EXPECT_EQ(out.mode, Mode::Blink);
    EXPECT_EQ(out.blink.color, Lamp::Green);
    EXPECT_EQ(out.blink.intervalMs, 300u);
    EXPECT_EQ(out.solid.color, saved.solid.color);  // other patterns untouched

    withObject(R"({"pattern":"chase","ttlMs":5000})", [&](JsonObjectConst obj) {
        Result r = parseModeCommand(obj, saved, out);
        EXPECT_FALSE(r.ok);
        EXPECT_STREQ(r.error, "ttlMs is only valid for overrides");
    });
}

TEST(SettingsCodecTest, PutQueryParamTypes) {
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    putQueryParam(obj, "intervalMs", "200");
    putQueryParam(obj, "color", "red");
    putQueryParam(obj, "neg", "-5");
    putQueryParam(obj, "big", "99999999999");
    putQueryParam(obj, "empty", "");
    EXPECT_TRUE(obj["intervalMs"].is<uint32_t>());
    EXPECT_EQ(obj["intervalMs"].as<uint32_t>(), 200u);
    EXPECT_STREQ(obj["color"], "red");
    EXPECT_TRUE(obj["neg"].is<const char*>());  // left for the parser to reject
    EXPECT_TRUE(obj["big"].is<const char*>());  // overflow stays a string
    EXPECT_TRUE(obj["empty"].is<const char*>());
}

TEST(SettingsCodecTest, QueryParamsFeedParseOverride) {
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    putQueryParam(obj, "color", "red");
    putQueryParam(obj, "intervalMs", "200");
    putQueryParam(obj, "ttlMs", "60000");
    obj["pattern"] = "blink";

    AppSettings saved;
    Override ov;
    ASSERT_TRUE(parseOverride(doc.as<JsonObjectConst>(), saved, ov).ok);
    EXPECT_EQ(ov.settings.blink.color, Lamp::Red);
    EXPECT_EQ(ov.settings.blink.intervalMs, 200u);
    EXPECT_EQ(ov.ttlMs, 60000u);

    putQueryParam(obj, "intervalMs", "12s");
    EXPECT_FALSE(parseOverride(doc.as<JsonObjectConst>(), saved, ov).ok);
}
