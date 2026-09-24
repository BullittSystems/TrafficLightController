/*
 * RandomMode.cpp
 *
 * Copyright (c) 2026 BullittSystems, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "RandomMode.h"

#include <Arduino.h>
#include <esp_random.h>
#include <string.h>

#include "StepMode.h"  // lampBit

namespace {
// Write the lit color names into buf ("RED+GREEN"), or "off" when the mask is 0.
void maskToNames(uint8_t mask, char* buf, size_t len) {
    struct LampName {
        uint8_t bit;
        const char* name;
    };
    const LampName lamps[] = {
        {lampBit(Lamp::Red), "RED"},
        {lampBit(Lamp::Yellow), "YELLOW"},
        {lampBit(Lamp::Green), "GREEN"},
    };
    buf[0] = '\0';
    for (const LampName& l : lamps) {
        if (mask & l.bit) {
            if (buf[0]) {
                strncat(buf, "+", len - strlen(buf) - 1);
            }
            strncat(buf, l.name, len - strlen(buf) - 1);
        }
    }
    if (!buf[0]) {
        strncpy(buf, "off", len);
    }
}
}  // namespace

RandomMode::RandomMode(LampController& lamps)
    : lamps_(lamps), currentMask_(0), stepStartMs_(0), stepDurationMs_(0) {}

void RandomMode::setSettings(const RandomSettings& settings) {
    settings_ = settings;
}

void RandomMode::begin() {
    hasCurrent_ = false;  // first pick is unconstrained
    pickNext();
}

void RandomMode::tick() {
    // Unsigned subtraction is rollover-safe across the millis() wrap.
    if (millis() - stepStartMs_ >= stepDurationMs_) {
        pickNext();
    }
}

uint8_t RandomMode::pickSingleMask() const {
    // Candidate single-lamp masks, excluding the current one so the same lamp never
    // repeats. (If the current mask is multi, no single bit matches it, so all three
    // are candidates.)
    uint8_t candidates[3];
    int n = 0;
    for (int i = 0; i < 3; i++) {
        const uint8_t bit = lampBit(static_cast<Lamp>(i));
        if (!hasCurrent_ || bit != currentMask_) {
            candidates[n++] = bit;
        }
    }
    return candidates[esp_random() % n];
}

uint8_t RandomMode::pickMultiMask() const {
    // Random non-empty subset of the three lamps, differing from the current.
    uint8_t mask;
    do {
        mask = static_cast<uint8_t>(esp_random() & 0x07);
    } while (mask == 0 || (hasCurrent_ && mask == currentMask_));
    return mask;
}

void RandomMode::pickNext() {
    const uint8_t mask =
        (settings_.variant == RandomVariant::Multi) ? pickMultiMask() : pickSingleMask();

    const uint32_t range = settings_.maxMs - settings_.minMs;
    const uint32_t duration =
        (range == 0) ? settings_.minMs : settings_.minMs + (esp_random() % (range + 1));

    currentMask_    = mask;
    hasCurrent_     = true;
    stepDurationMs_ = duration;
    stepStartMs_    = millis();

    lamps_.set(Lamp::Red, (mask & lampBit(Lamp::Red)) != 0);
    lamps_.set(Lamp::Yellow, (mask & lampBit(Lamp::Yellow)) != 0);
    lamps_.set(Lamp::Green, (mask & lampBit(Lamp::Green)) != 0);

    char names[24];
    maskToNames(mask, names, sizeof(names));
    Serial.printf("random -> %s (%lums)\n", names, (unsigned long) duration);
}
