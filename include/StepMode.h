/*
 * StepMode.h
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

// One step of a lamp pattern: a set of lamps held for a duration.
struct Step {
    uint8_t lampMask;     // bit (1 << int(Lamp)) per lit lamp; 0 = all off
    uint32_t durationMs;  // 0 = hold this step indefinitely (no advance)
};

// Maximum steps in a pattern (Sequence uses 3, Blink 2, Solid/Off 1).
static constexpr uint8_t MAX_STEPS = 4;

// Bit mask for a single lamp, for building Step tables.
static constexpr uint8_t lampBit(Lamp lamp) {
    return static_cast<uint8_t>(1u << static_cast<int>(lamp));
}

// Non-blocking engine that walks a small step table and loops. Backs every
// table-based mode (Sequence, Solid, Blink, Off); the table is supplied by the
// caller. Copies the table into a fixed internal buffer, so no ownership or heap.
class StepMode {
  public:
    explicit StepMode(LampController& lamps);

    // Replace the step table (copied, up to MAX_STEPS). Takes effect on the next
    // advance without restarting, unless the current index is now out of range.
    void setSteps(const Step* steps, uint8_t count);

    // Restart at step 0 and apply it. Call after a mode change.
    void begin();

    // Advance to the next step if the current one has elapsed. Call every loop().
    void tick();

  private:
    void applyStep(uint8_t index);

    LampController& lamps_;
    Step steps_[MAX_STEPS];
    uint8_t count_;
    uint8_t index_;
    uint32_t stepStartMs_;
};
