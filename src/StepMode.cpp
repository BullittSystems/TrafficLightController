/*
 * StepMode.cpp
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
#include "StepMode.h"

#include <Arduino.h>

namespace {
const Lamp ALL_LAMPS[] = {Lamp::Red, Lamp::Yellow, Lamp::Green};

const char* maskName(uint8_t mask) {
    switch (mask) {
        case lampBit(Lamp::Red):
            return "RED";
        case lampBit(Lamp::Yellow):
            return "YELLOW";
        case lampBit(Lamp::Green):
            return "GREEN";
        case 0:
            return "off";
        default:
            return "multi";
    }
}
}  // namespace

StepMode::StepMode(LampController& lamps) : lamps_(lamps), count_(0), index_(0), stepStartMs_(0) {}

void StepMode::setSteps(const Step* steps, uint8_t count) {
    if (count > MAX_STEPS) {
        count = MAX_STEPS;
    }
    for (uint8_t i = 0; i < count; i++) {
        steps_[i] = steps[i];
    }
    count_ = count;
    if (index_ >= count_) {
        index_ = 0;
    }
    // Re-apply the current step so a live table change (e.g. editing the color of
    // a hold-forever step) takes effect immediately. Timing is left untouched, so
    // Sequence/Blink keep their cadence rather than restarting.
    if (count_ > 0) {
        applyStep(index_);
    }
}

void StepMode::begin() {
    index_       = 0;
    stepStartMs_ = millis();
    applyStep(index_);
}

void StepMode::tick() {
    if (count_ == 0) {
        return;
    }
    const uint32_t duration = steps_[index_].durationMs;
    if (duration == 0) {
        return;  // hold this step indefinitely
    }
    // Unsigned subtraction is rollover-safe across the millis() wrap.
    if (millis() - stepStartMs_ >= duration) {
        index_       = (index_ + 1) % count_;
        stepStartMs_ = millis();
        applyStep(index_);
    }
}

void StepMode::applyStep(uint8_t index) {
    const uint8_t mask = steps_[index].lampMask;
    for (Lamp lamp : ALL_LAMPS) {
        lamps_.set(lamp, (mask & lampBit(lamp)) != 0);
    }
    Serial.printf("step -> %s\n", maskName(mask));
}
