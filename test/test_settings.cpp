/*
 * test_settings.cpp
 *
 * Native unit tests for Settings (NVS load/save round-trip, defaults, guards).
 */
#include <gtest/gtest.h>

#include "Preferences.h"
#include "Settings.h"
#include "config.h"

class SettingsTest : public ::testing::Test {
  protected:
    Settings settings;

    void SetUp() override { mock_prefs_clear(); }
};

TEST_F(SettingsTest, LoadsDefaultsOnVirginNvs) {
    AppSettings s = settings.load();
    EXPECT_EQ(s.mode, Mode::Sequence);
    EXPECT_EQ(s.sequence.greenMs, DEFAULT_GREEN_MS);
    EXPECT_EQ(s.sequence.yellowMs, DEFAULT_YELLOW_MS);
    EXPECT_EQ(s.sequence.redMs, DEFAULT_RED_MS);
    EXPECT_EQ(s.random.minMs, DEFAULT_RANDOM_MIN_MS);
    EXPECT_EQ(s.random.maxMs, DEFAULT_RANDOM_MAX_MS);
    EXPECT_EQ(s.random.variant, RandomVariant::Single);
    EXPECT_EQ(s.solid.color, Lamp::Green);
    EXPECT_EQ(s.blink.color, Lamp::Red);
    EXPECT_EQ(s.blink.intervalMs, DEFAULT_BLINK_MS);
}

TEST_F(SettingsTest, RoundTripsAllFields) {
    AppSettings out;
    out.mode              = Mode::Blink;
    out.sequence.greenMs  = 1111;
    out.sequence.yellowMs = 2222;
    out.sequence.redMs    = 3333;
    out.random.minMs      = 400;
    out.random.maxMs      = 4000;
    out.random.variant    = RandomVariant::Multi;
    out.solid.color       = Lamp::Yellow;
    out.blink.color       = Lamp::Green;
    out.blink.intervalMs  = 250;
    settings.save(out);

    AppSettings in = settings.load();
    EXPECT_EQ(in.mode, Mode::Blink);
    EXPECT_EQ(in.sequence.greenMs, 1111u);
    EXPECT_EQ(in.sequence.yellowMs, 2222u);
    EXPECT_EQ(in.sequence.redMs, 3333u);
    EXPECT_EQ(in.random.minMs, 400u);
    EXPECT_EQ(in.random.maxMs, 4000u);
    EXPECT_EQ(in.random.variant, RandomVariant::Multi);
    EXPECT_EQ(in.solid.color, Lamp::Yellow);
    EXPECT_EQ(in.blink.color, Lamp::Green);
    EXPECT_EQ(in.blink.intervalMs, 250u);
}

TEST_F(SettingsTest, ApiModePersists) {
    AppSettings out;
    out.mode = Mode::Api;
    settings.save(out);
    EXPECT_EQ(settings.load().mode, Mode::Api);
}

TEST_F(SettingsTest, OutOfRangeModeCoercedToSequence) {
    // Write a bogus persisted mode byte directly, then load.
    Preferences prefs;
    prefs.begin("traflight", false);
    prefs.putUChar("mode", 99);
    prefs.end();

    EXPECT_EQ(settings.load().mode, Mode::Sequence);
}

TEST_F(SettingsTest, InvalidColorCoercedToDefault) {
    Preferences prefs;
    prefs.begin("traflight", false);
    prefs.putUChar("solid_color", 42);
    prefs.putUChar("blink_color", 42);
    prefs.end();

    AppSettings s = settings.load();
    EXPECT_EQ(s.solid.color, Lamp::Green);  // default
    EXPECT_EQ(s.blink.color, Lamp::Red);    // default
}

TEST_F(SettingsTest, InvalidVariantCoercedToSingle) {
    Preferences prefs;
    prefs.begin("traflight", false);
    prefs.putUChar("rnd_variant", 7);
    prefs.end();

    EXPECT_EQ(settings.load().random.variant, RandomVariant::Single);
}
