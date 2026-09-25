/*
 * test_modecontroller.cpp
 *
 * Native unit tests for ModeController (mode dispatch, overrides and their expiry).
 */
#include <gtest/gtest.h>

#include "Arduino.h"
#include "LampController.h"
#include "ModeController.h"
#include "RandomMode.h"
#include "StepMode.h"
#include "config.h"

namespace {
bool redOn() {
    return mock_pin_state[RED_GPIO] != 0;
}
bool yellowOn() {
    return mock_pin_state[YELLOW_GPIO] != 0;
}
bool greenOn() {
    return mock_pin_state[GREEN_GPIO] != 0;
}
bool allOff() {
    return !redOn() && !yellowOn() && !greenOn();
}
}  // namespace

class ModeControllerTest : public ::testing::Test {
  protected:
    LampController lamps;
    StepMode step{lamps};
    RandomMode random{lamps};
    ModeController controller{step, random};

    void SetUp() override {
        mock_reset();
        lamps.begin();
    }

    AppSettings base() {
        AppSettings s;  // compile-time defaults
        return s;
    }
};

TEST_F(ModeControllerTest, SequenceStartsOnGreenAndCycles) {
    AppSettings s = base();
    s.mode        = Mode::Sequence;
    controller.apply(s);
    EXPECT_TRUE(greenOn());

    mock_advance_millis(s.sequence.greenMs);
    controller.tick();
    EXPECT_TRUE(yellowOn());
}

TEST_F(ModeControllerTest, SolidLightsConfiguredColor) {
    AppSettings s = base();
    s.mode        = Mode::Solid;
    s.solid.color = Lamp::Green;
    controller.apply(s);
    EXPECT_TRUE(greenOn());
    EXPECT_FALSE(redOn());
}

TEST_F(ModeControllerTest, OffLeavesAllLampsDark) {
    AppSettings s = base();
    s.mode        = Mode::Off;
    controller.apply(s);
    EXPECT_TRUE(allOff());
}

TEST_F(ModeControllerTest, BlinkToggles) {
    AppSettings s      = base();
    s.mode             = Mode::Blink;
    s.blink.color      = Lamp::Red;
    s.blink.intervalMs = 500;
    controller.apply(s);
    EXPECT_TRUE(redOn());

    mock_advance_millis(500);
    controller.tick();
    EXPECT_FALSE(redOn());
}

TEST_F(ModeControllerTest, ChaseWalksSixOverlappingSteps) {
    AppSettings s      = base();
    s.mode             = Mode::Chase;
    s.chase.intervalMs = 100;
    controller.apply(s);

    auto expectLamps = [](bool r, bool y, bool g) {
        EXPECT_EQ(redOn(), r);
        EXPECT_EQ(yellowOn(), y);
        EXPECT_EQ(greenOn(), g);
    };

    expectLamps(true, false, false);  // R
    mock_advance_millis(100);
    controller.tick();
    expectLamps(true, true, false);  // R+Y
    mock_advance_millis(100);
    controller.tick();
    expectLamps(false, true, false);  // Y
    mock_advance_millis(100);
    controller.tick();
    expectLamps(false, true, true);  // Y+G
    mock_advance_millis(100);
    controller.tick();
    expectLamps(false, false, true);  // G
    mock_advance_millis(100);
    controller.tick();
    expectLamps(true, false, true);  // G+R
    mock_advance_millis(100);
    controller.tick();
    expectLamps(true, false, false);  // wraps to R
}

TEST_F(ModeControllerTest, ParamEditAppliesImmediatelyWithoutModeChange) {
    AppSettings s = base();
    s.mode        = Mode::Solid;
    s.solid.color = Lamp::Green;
    controller.apply(s);
    EXPECT_TRUE(greenOn());

    // Same mode, different color: must reflect at once (refresh path).
    s.solid.color = Lamp::Red;
    controller.apply(s);
    EXPECT_TRUE(redOn());
    EXPECT_FALSE(greenOn());
}

TEST_F(ModeControllerTest, CurrentMaskReflectsLitLamp) {
    AppSettings s = base();
    s.mode        = Mode::Solid;
    s.solid.color = Lamp::Red;
    controller.apply(s);
    EXPECT_EQ(controller.currentMask(), lampBit(Lamp::Red));
}

TEST_F(ModeControllerTest, CurrentMaskReflectsRandomMode) {
    AppSettings s = base();
    s.mode        = Mode::Random;
    controller.apply(s);
    // Random single lights exactly one lamp; currentMask must match the lit pin.
    const uint8_t mask = controller.currentMask();
    EXPECT_EQ(mask, static_cast<uint8_t>((redOn() ? lampBit(Lamp::Red) : 0) |
                                         (yellowOn() ? lampBit(Lamp::Yellow) : 0) |
                                         (greenOn() ? lampBit(Lamp::Green) : 0)));
    EXPECT_NE(mask, 0);
}

TEST_F(ModeControllerTest, ModeChangeRestartsSequenceFromGreen) {
    AppSettings s = base();
    s.mode        = Mode::Sequence;
    controller.apply(s);
    mock_advance_millis(s.sequence.greenMs);
    controller.tick();  // now on yellow

    // Leave and re-enter sequence: it should restart at green, not resume yellow.
    AppSettings off = base();
    off.mode        = Mode::Off;
    controller.apply(off);
    controller.apply(s);
    EXPECT_TRUE(greenOn());
    EXPECT_FALSE(yellowOn());
}

namespace {
Override solidOverride(const AppSettings& saved, Lamp color, uint32_t ttlMs = 0) {
    Override ov;
    ov.settings             = saved;
    ov.settings.mode        = Mode::Solid;
    ov.settings.solid.color = color;
    ov.ttlMs                = ttlMs;
    return ov;
}
}  // namespace

TEST_F(ModeControllerTest, OverrideCoversModeAndClearRestoresIt) {
    AppSettings s = base();
    s.mode        = Mode::Sequence;
    controller.apply(s);
    EXPECT_TRUE(greenOn());

    controller.setOverride(solidOverride(s, Lamp::Red));
    EXPECT_TRUE(redOn());
    EXPECT_FALSE(greenOn());

    // A solid override holds while the mode's timing would have advanced.
    mock_advance_millis(s.sequence.greenMs);
    controller.tick();
    EXPECT_TRUE(redOn());

    EXPECT_TRUE(controller.clearOverride());
    EXPECT_TRUE(greenOn());  // mode restarts from its first step
    EXPECT_FALSE(redOn());
    EXPECT_FALSE(controller.clearOverride());  // nothing left to clear
}

TEST_F(ModeControllerTest, OverrideWorksFromOffMode) {
    AppSettings s = base();
    s.mode        = Mode::Off;
    controller.apply(s);
    EXPECT_TRUE(allOff());

    controller.setOverride(solidOverride(s, Lamp::Yellow));
    EXPECT_TRUE(yellowOn());
}

TEST_F(ModeControllerTest, TimedOverrideExpiresBackToMode) {
    AppSettings s = base();
    s.mode        = Mode::Solid;
    s.solid.color = Lamp::Green;
    controller.apply(s);

    controller.setOverride(solidOverride(s, Lamp::Red, 5000));
    mock_advance_millis(4999);
    controller.tick();
    EXPECT_TRUE(redOn());
    EXPECT_EQ(controller.status().remainingMs, 1u);

    mock_advance_millis(1);
    controller.tick();
    EXPECT_TRUE(greenOn());
    EXPECT_FALSE(redOn());
    EXPECT_FALSE(controller.status().overridden);
}

TEST_F(ModeControllerTest, TimedOverrideSurvivesMillisRollover) {
    mock_set_millis(0xFFFFFF00u);
    AppSettings s = base();
    s.mode        = Mode::Off;
    controller.apply(s);
    controller.setOverride(solidOverride(s, Lamp::Red, 1000));

    mock_advance_millis(0x200);  // wraps past zero; 512 ms elapsed
    controller.tick();
    EXPECT_TRUE(redOn());
    mock_advance_millis(488);
    controller.tick();
    EXPECT_TRUE(allOff());
}

TEST_F(ModeControllerTest, SettingModeClearsOverride) {
    AppSettings s = base();
    s.mode        = Mode::Solid;
    s.solid.color = Lamp::Green;
    controller.apply(s);
    controller.setOverride(solidOverride(s, Lamp::Red));

    s.mode = Mode::Chase;
    controller.apply(s, /*clearOverride=*/true);
    EXPECT_FALSE(controller.status().overridden);
    EXPECT_TRUE(redOn());  // chase starts on red
    EXPECT_FALSE(greenOn());
}

TEST_F(ModeControllerTest, SettingsWithoutModeKeepOverride) {
    AppSettings s = base();
    s.mode        = Mode::Solid;
    s.solid.color = Lamp::Green;
    controller.apply(s);
    controller.setOverride(solidOverride(s, Lamp::Red));

    // A parameter-only save updates the baseline but leaves the override showing.
    s.solid.color = Lamp::Yellow;
    controller.apply(s);
    EXPECT_TRUE(redOn());
    EXPECT_TRUE(controller.status().overridden);

    controller.clearOverride();
    EXPECT_TRUE(yellowOn());  // new baseline shows once the override ends
}

TEST_F(ModeControllerTest, RandomOverrideOverStepMode) {
    AppSettings s = base();
    s.mode        = Mode::Off;
    controller.apply(s);

    Override ov;
    ov.settings      = s;
    ov.settings.mode = Mode::Random;
    controller.setOverride(ov);
    EXPECT_NE(controller.currentMask(), 0);  // random single lights one lamp
    EXPECT_FALSE(allOff());

    controller.clearOverride();
    EXPECT_TRUE(allOff());
    EXPECT_EQ(controller.currentMask(), 0);
}

TEST_F(ModeControllerTest, StatusReportsModeAndOverride) {
    AppSettings s = base();
    s.mode        = Mode::Chase;
    controller.apply(s);
    EXPECT_EQ(controller.status().mode, Mode::Chase);
    EXPECT_FALSE(controller.status().overridden);

    controller.setOverride(solidOverride(s, Lamp::Green));
    const LiveStatus st = controller.status();
    EXPECT_EQ(st.mode, Mode::Chase);
    EXPECT_TRUE(st.overridden);
    EXPECT_EQ(st.active.settings.mode, Mode::Solid);
    EXPECT_EQ(st.litMask, lampBit(Lamp::Green));
    EXPECT_EQ(st.remainingMs, 0u);  // untimed
}
