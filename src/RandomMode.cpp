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

namespace {
const char* lampName(Lamp lamp) {
    switch (lamp) {
        case Lamp::Red:
            return "RED";
        case Lamp::Yellow:
            return "YELLOW";
        case Lamp::Green:
            return "GREEN";
    }
    return "?";
}
}  // namespace

RandomMode::RandomMode(LampController& lamps)
    : lamps_(lamps), current_(Lamp::Red), stepStartMs_(0), stepDurationMs_(0) {}

void RandomMode::setBounds(const RandomBounds& bounds) {
    bounds_ = bounds;
}

void RandomMode::begin() {
    hasCurrent_ = false;  // first pick chooses among all three lamps
    pickNext();
}

void RandomMode::tick() {
    // Unsigned subtraction is rollover-safe across the millis() wrap.
    if (millis() - stepStartMs_ >= stepDurationMs_) {
        pickNext();
    }
}

void RandomMode::pickNext() {
    Lamp next;
    if (!hasCurrent_) {
        // No lamp lit yet: choose uniformly among all three.
        next = static_cast<Lamp>(esp_random() % 3);
    } else {
        // Choose uniformly among the two lamps that are not currently lit, so the
        // same lamp never repeats.
        Lamp candidates[2];
        int n = 0;
        for (int i = 0; i < 3; i++) {
            Lamp lamp = static_cast<Lamp>(i);
            if (lamp != current_) {
                candidates[n++] = lamp;
            }
        }
        next = candidates[esp_random() % 2];
    }

    const uint32_t range = bounds_.maxMs - bounds_.minMs;
    const uint32_t duration =
        (range == 0) ? bounds_.minMs : bounds_.minMs + (esp_random() % (range + 1));

    current_        = next;
    hasCurrent_     = true;
    stepDurationMs_ = duration;
    stepStartMs_    = millis();

    lamps_.allOff();
    lamps_.set(current_, true);
    Serial.printf("random -> %s (%lums)\n", lampName(current_), (unsigned long) duration);
}
