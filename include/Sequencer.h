/*
 * Sequencer.h
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
#pragma once

#include <stdint.h>

#include "LampController.h"

// Per-phase durations in milliseconds. Defaults are placeholders; these become
// settings-driven in a later task.
struct PhaseDurations {
    uint32_t greenMs  = 3000;
    uint32_t yellowMs = 3000;
    uint32_t redMs    = 3000;
};

// Non-blocking traffic-light state machine. Cycles GREEN -> YELLOW -> RED and
// repeats, advancing on elapsed millis(). Exactly one lamp is lit per phase.
class Sequencer {
  public:
    explicit Sequencer(LampController& lamps);

    // Apply the initial phase and start timing. Call once after LampController::begin().
    void begin();

    // Advance the state machine if the current phase has elapsed. Call every loop().
    void tick();

  private:
    enum class Phase { Green, Yellow, Red };

    void applyPhase(Phase phase);
    Phase nextPhase(Phase phase) const;
    uint32_t durationFor(Phase phase) const;
    static const char* phaseName(Phase phase);

    LampController& lamps_;
    PhaseDurations durations_;
    Phase phase_;
    uint32_t phaseStartMs_;
};
