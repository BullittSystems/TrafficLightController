/*
 * test_modecontroller.cpp
 *
 * Native unit tests for ModeController (mode dispatch, API state gating, idle).
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

TEST_F(ModeControllerTest, ApiModeIdleShowsYellowBlink) {
    AppSettings s = base();
    s.mode        = Mode::Api;
    controller.apply(s);

    // Idle = blink yellow: on-phase lit immediately.
    EXPECT_TRUE(yellowOn());
    EXPECT_FALSE(redOn());
    EXPECT_FALSE(greenOn());

    mock_advance_millis(DEFAULT_BLINK_MS);
    controller.tick();
    EXPECT_TRUE(allOff());  // off-phase
}

TEST_F(ModeControllerTest, ApiStateRejectedWhenNotInApiMode) {
    AppSettings s = base();
    s.mode        = Mode::Solid;
    s.solid.color = Lamp::Green;
    controller.apply(s);

    ApiState cmd;
    cmd.kind  = ApiStateKind::Solid;
    cmd.color = Lamp::Red;
    EXPECT_FALSE(controller.applyApiState(cmd));
    EXPECT_TRUE(greenOn());  // unchanged
}

TEST_F(ModeControllerTest, ApiStateAppliedWhenInApiMode) {
    AppSettings s = base();
    s.mode        = Mode::Api;
    controller.apply(s);

    ApiState cmd;
    cmd.kind  = ApiStateKind::Solid;
    cmd.color = Lamp::Red;
    EXPECT_TRUE(controller.applyApiState(cmd));
    EXPECT_TRUE(redOn());
    EXPECT_FALSE(yellowOn());
}

TEST_F(ModeControllerTest, CurrentMaskReflectsLitLamp) {
    AppSettings s = base();
    s.mode        = Mode::Solid;
    s.solid.color = Lamp::Red;
    controller.apply(s);
    EXPECT_EQ(controller.currentMask(), lampBit(Lamp::Red));
}
