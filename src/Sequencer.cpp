/*
 * Sequencer.cpp
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
#include "Sequencer.h"

#include <Arduino.h>

Sequencer::Sequencer(LampController& lamps)
    : lamps_(lamps), phase_(Phase::Green), phaseStartMs_(0) {}

void Sequencer::begin() {
    phase_        = Phase::Green;
    phaseStartMs_ = millis();
    applyPhase(phase_);
}

void Sequencer::tick() {
    // Unsigned subtraction is rollover-safe across the millis() wrap.
    if (millis() - phaseStartMs_ >= durationFor(phase_)) {
        phase_        = nextPhase(phase_);
        phaseStartMs_ = millis();
        applyPhase(phase_);
    }
}

void Sequencer::applyPhase(Phase phase) {
    lamps_.allOff();
    switch (phase) {
        case Phase::Green:
            lamps_.set(Lamp::Green, true);
            break;
        case Phase::Yellow:
            lamps_.set(Lamp::Yellow, true);
            break;
        case Phase::Red:
            lamps_.set(Lamp::Red, true);
            break;
    }
    Serial.printf("phase -> %s\n", phaseName(phase));
}

Sequencer::Phase Sequencer::nextPhase(Phase phase) const {
    switch (phase) {
        case Phase::Green:
            return Phase::Yellow;
        case Phase::Yellow:
            return Phase::Red;
        case Phase::Red:
            return Phase::Green;
    }
    return Phase::Red;  // unreachable; safe default is the all-stop phase
}

uint32_t Sequencer::durationFor(Phase phase) const {
    switch (phase) {
        case Phase::Green:
            return durations_.greenMs;
        case Phase::Yellow:
            return durations_.yellowMs;
        case Phase::Red:
            return durations_.redMs;
    }
    return durations_.redMs;  // unreachable
}

const char* Sequencer::phaseName(Phase phase) {
    switch (phase) {
        case Phase::Green:
            return "GREEN";
        case Phase::Yellow:
            return "YELLOW";
        case Phase::Red:
            return "RED";
    }
    return "?";
}
