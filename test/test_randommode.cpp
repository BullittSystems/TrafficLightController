/*
 * test_randommode.cpp
 *
 * Native unit tests for RandomMode (single/multi variants + duration bounds).
 */
#include <gtest/gtest.h>

#include "Arduino.h"
#include "LampController.h"
#include "RandomMode.h"
#include "StepMode.h"  // lampBit
#include "config.h"
#include "esp_random.h"

namespace {
int countBits(uint8_t mask) {
    int n = 0;
    for (int i = 0; i < 3; i++) {
        if (mask & (1u << i))
            n++;
    }
    return n;
}
}  // namespace

class RandomModeTest : public ::testing::Test {
  protected:
    LampController lamps;
    RandomMode rnd{lamps};

    void SetUp() override {
        mock_reset();
        mock_rng_reset();
        lamps.begin();
    }
};

TEST_F(RandomModeTest, SingleLightsExactlyOneAndNeverRepeats) {
    RandomSettings s;
    s.variant = RandomVariant::Single;
    s.minMs   = 100;
    s.maxMs   = 100;
    rnd.setSettings(s);
    rnd.begin();

    uint8_t prev = rnd.currentMask();
    EXPECT_EQ(countBits(prev), 1);

    for (int i = 0; i < 200; i++) {
        mock_advance_millis(100);
        rnd.tick();
        const uint8_t m = rnd.currentMask();
        EXPECT_EQ(countBits(m), 1);
        EXPECT_NE(m, prev);
        prev = m;
    }
}

TEST_F(RandomModeTest, MultiLightsNonEmptySubsetAndAlwaysChanges) {
    RandomSettings s;
    s.variant = RandomVariant::Multi;
    s.minMs   = 100;
    s.maxMs   = 100;
    rnd.setSettings(s);
    rnd.begin();

    uint8_t prev = rnd.currentMask();
    EXPECT_GE(countBits(prev), 1);
    EXPECT_LE(countBits(prev), 3);

    for (int i = 0; i < 200; i++) {
        mock_advance_millis(100);
        rnd.tick();
        const uint8_t m = rnd.currentMask();
        EXPECT_GE(countBits(m), 1);
        EXPECT_LE(countBits(m), 3);
        EXPECT_NE(m, prev);
        prev = m;
    }
}

TEST_F(RandomModeTest, LampPinsMatchCurrentMask) {
    RandomSettings s;
    s.variant = RandomVariant::Multi;
    rnd.setSettings(s);
    rnd.begin();

    const uint8_t m = rnd.currentMask();
    EXPECT_EQ(mock_pin_state[RED_GPIO] != 0, (m & lampBit(Lamp::Red)) != 0);
    EXPECT_EQ(mock_pin_state[YELLOW_GPIO] != 0, (m & lampBit(Lamp::Yellow)) != 0);
    EXPECT_EQ(mock_pin_state[GREEN_GPIO] != 0, (m & lampBit(Lamp::Green)) != 0);
}

TEST_F(RandomModeTest, FixedIntervalHoldsExactly) {
    RandomSettings s;
    s.variant = RandomVariant::Single;
    s.minMs   = 1000;
    s.maxMs   = 1000;
    rnd.setSettings(s);
    rnd.begin();
    const uint8_t prev = rnd.currentMask();

    mock_advance_millis(999);
    rnd.tick();
    EXPECT_EQ(rnd.currentMask(), prev);  // not elapsed

    mock_advance_millis(1);
    rnd.tick();
    EXPECT_NE(rnd.currentMask(), prev);  // elapsed at exactly 1000
}

TEST_F(RandomModeTest, HoldStaysWithinBounds) {
    RandomSettings s;
    s.variant = RandomVariant::Single;
    s.minMs   = 500;
    s.maxMs   = 1500;
    rnd.setSettings(s);
    rnd.begin();
    const uint8_t prev = rnd.currentMask();

    // Below the minimum it must not have advanced.
    mock_advance_millis(499);
    rnd.tick();
    EXPECT_EQ(rnd.currentMask(), prev);

    // By the maximum it must have advanced.
    mock_advance_millis(1001);  // total elapsed 1500
    rnd.tick();
    EXPECT_NE(rnd.currentMask(), prev);
}
