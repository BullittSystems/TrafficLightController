/*
 * test_stepmode.cpp
 *
 * Native unit tests for StepMode + LampController (the shared pattern engine).
 */
#include <gtest/gtest.h>

#include "Arduino.h"
#include "LampController.h"
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
}  // namespace

class StepModeTest : public ::testing::Test {
  protected:
    LampController lamps;
    StepMode step{lamps};

    void SetUp() override {
        mock_reset();
        lamps.begin();
    }
};

TEST_F(StepModeTest, SolidLightsOneLampAndHoldsForever) {
    Step steps[] = {{lampBit(Lamp::Green), 0}};
    step.setSteps(steps, 1);
    step.begin();

    EXPECT_TRUE(greenOn());
    EXPECT_FALSE(redOn());
    EXPECT_FALSE(yellowOn());

    // duration 0 never advances, even after a long time
    mock_advance_millis(1000000);
    step.tick();
    EXPECT_TRUE(greenOn());
}

TEST_F(StepModeTest, BlinkTogglesAtInterval) {
    Step steps[] = {{lampBit(Lamp::Red), 500}, {0, 500}};
    step.setSteps(steps, 2);
    step.begin();
    EXPECT_TRUE(redOn());

    mock_advance_millis(499);
    step.tick();
    EXPECT_TRUE(redOn());  // not elapsed yet

    mock_advance_millis(1);
    step.tick();
    EXPECT_FALSE(redOn());  // off phase

    mock_advance_millis(500);
    step.tick();
    EXPECT_TRUE(redOn());  // back on
}

TEST_F(StepModeTest, SequenceCyclesGreenYellowRedAndLoops) {
    Step steps[] = {
        {lampBit(Lamp::Green), 1000}, {lampBit(Lamp::Yellow), 1000}, {lampBit(Lamp::Red), 1000}};
    step.setSteps(steps, 3);
    step.begin();
    EXPECT_TRUE(greenOn());

    mock_advance_millis(1000);
    step.tick();
    EXPECT_TRUE(yellowOn());
    EXPECT_FALSE(greenOn());

    mock_advance_millis(1000);
    step.tick();
    EXPECT_TRUE(redOn());
    EXPECT_FALSE(yellowOn());

    mock_advance_millis(1000);
    step.tick();
    EXPECT_TRUE(greenOn());  // looped
    EXPECT_FALSE(redOn());
}

TEST_F(StepModeTest, MultiLampStepLightsAllBitsInMask) {
    Step steps[] = {{static_cast<uint8_t>(lampBit(Lamp::Red) | lampBit(Lamp::Green)), 0}};
    step.setSteps(steps, 1);
    step.begin();

    EXPECT_TRUE(redOn());
    EXPECT_TRUE(greenOn());
    EXPECT_FALSE(yellowOn());
}

TEST_F(StepModeTest, RefreshAppliesNewTableWithoutAdvancing) {
    Step green[] = {{lampBit(Lamp::Green), 0}};
    step.setSteps(green, 1);
    step.begin();
    EXPECT_TRUE(greenOn());

    // Change the (hold-forever) table; setSteps alone must not touch the lamps.
    Step red[] = {{lampBit(Lamp::Red), 0}};
    step.setSteps(red, 1);
    EXPECT_TRUE(greenOn());
    EXPECT_FALSE(redOn());

    // refresh re-applies the current step in place.
    step.refresh();
    EXPECT_TRUE(redOn());
    EXPECT_FALSE(greenOn());
}

TEST_F(StepModeTest, TimingIsRolloverSafe) {
    mock_set_millis(0xFFFFFF00u);
    Step steps[] = {{lampBit(Lamp::Red), 0x200}, {0, 0x200}};
    step.setSteps(steps, 2);
    step.begin();
    EXPECT_TRUE(redOn());

    // Wrap past UINT32_MAX; elapsed is still 0x200 via unsigned subtraction.
    mock_set_millis(0x100u);
    step.tick();
    EXPECT_FALSE(redOn());
}

TEST_F(StepModeTest, SetStepsClampsStaleIndex) {
    Step seq[] = {
        {lampBit(Lamp::Green), 1000}, {lampBit(Lamp::Yellow), 1000}, {lampBit(Lamp::Red), 1000}};
    step.setSteps(seq, 3);
    step.begin();
    mock_advance_millis(1000);
    step.tick();
    mock_advance_millis(1000);
    step.tick();  // now at index 2 (red)
    EXPECT_TRUE(redOn());

    // Shrink to a 1-step table: the stale index 2 must clamp to 0, not read OOB.
    Step green[] = {{lampBit(Lamp::Green), 0}};
    step.setSteps(green, 1);
    step.refresh();
    EXPECT_TRUE(greenOn());
    EXPECT_FALSE(redOn());
}
