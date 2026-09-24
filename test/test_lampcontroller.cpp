/*
 * test_lampcontroller.cpp
 *
 * Native unit tests for LampController (boot-safe init + on/off control).
 */
#include <gtest/gtest.h>

#include "Arduino.h"
#include "LampController.h"
#include "config.h"

class LampControllerTest : public ::testing::Test {
  protected:
    LampController lamps;
    void SetUp() override { mock_reset(); }
};

TEST_F(LampControllerTest, BeginConfiguresOutputsAndDrivesAllLow) {
    // Pre-dirty the pins to prove begin() forces them low.
    mock_pin_state[RED_GPIO]    = HIGH;
    mock_pin_state[YELLOW_GPIO] = HIGH;
    mock_pin_state[GREEN_GPIO]  = HIGH;

    lamps.begin();

    EXPECT_EQ(mock_pin_mode[RED_GPIO], OUTPUT);
    EXPECT_EQ(mock_pin_mode[YELLOW_GPIO], OUTPUT);
    EXPECT_EQ(mock_pin_mode[GREEN_GPIO], OUTPUT);

    EXPECT_EQ(mock_pin_state[RED_GPIO], LOW);
    EXPECT_EQ(mock_pin_state[YELLOW_GPIO], LOW);
    EXPECT_EQ(mock_pin_state[GREEN_GPIO], LOW);
}

TEST_F(LampControllerTest, SetTogglesOnlyTheTargetLamp) {
    lamps.begin();

    lamps.set(Lamp::Yellow, true);
    EXPECT_EQ(mock_pin_state[YELLOW_GPIO], HIGH);
    EXPECT_EQ(mock_pin_state[RED_GPIO], LOW);
    EXPECT_EQ(mock_pin_state[GREEN_GPIO], LOW);

    lamps.set(Lamp::Yellow, false);
    EXPECT_EQ(mock_pin_state[YELLOW_GPIO], LOW);
}

TEST_F(LampControllerTest, AllOffClearsEveryLamp) {
    lamps.begin();
    lamps.set(Lamp::Red, true);
    lamps.set(Lamp::Green, true);

    lamps.allOff();

    EXPECT_EQ(mock_pin_state[RED_GPIO], LOW);
    EXPECT_EQ(mock_pin_state[YELLOW_GPIO], LOW);
    EXPECT_EQ(mock_pin_state[GREEN_GPIO], LOW);
}
