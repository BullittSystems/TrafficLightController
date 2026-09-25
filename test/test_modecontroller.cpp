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

TEST_F(ModeControllerTest, ReenteringApiResetsToIdle) {
    AppSettings s = base();
    s.mode        = Mode::Api;
    controller.apply(s);

    // Drive it to solid red via an API command.
    ApiState cmd;
    cmd.kind  = ApiStateKind::Solid;
    cmd.color = Lamp::Red;
    ASSERT_TRUE(controller.applyApiState(cmd));
    EXPECT_TRUE(redOn());

    // Switch away and back: API state must reset to the yellow-blink idle.
    AppSettings off = base();
    off.mode        = Mode::Off;
    controller.apply(off);
    controller.apply(s);

    EXPECT_TRUE(yellowOn());
    EXPECT_FALSE(redOn());
    EXPECT_EQ(controller.apiState().kind, ApiStateKind::Blink);
    EXPECT_EQ(controller.apiState().color, Lamp::Yellow);
}
