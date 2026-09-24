/*
 * RandomMode.h
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
#include "config.h"

// Non-blocking random light mode. Lights exactly one lamp at a time; each step
// picks a lamp uniformly at random from the two not currently lit and holds it
// for a uniform-random duration in [minMs, maxMs].
class RandomMode {
  public:
    explicit RandomMode(LampController& lamps);

    // Set the hold-time bounds. Takes effect on the next step; safe to call at runtime.
    void setBounds(const RandomBounds& bounds);

    // Pick an initial lamp/duration and start. Call after LampController::begin().
    void begin();

    // Advance to a new random lamp/duration if the current step has elapsed.
    void tick();

  private:
    void pickNext();

    LampController& lamps_;
    RandomBounds bounds_;
    Lamp current_;
    uint32_t stepStartMs_;
    uint32_t stepDurationMs_;
    bool hasCurrent_ = false;
};
