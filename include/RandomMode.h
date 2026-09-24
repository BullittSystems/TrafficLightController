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

// Non-blocking random light mode. Each step lights a random target and holds it
// for a uniform-random duration in [minMs, maxMs], then picks a new one that
// differs from the current. In Single variant the target is one lamp; in Multi it
// is a random non-empty subset (1-3 lamps).
class RandomMode {
  public:
    explicit RandomMode(LampController& lamps);

    // Set bounds + variant. Takes effect on the next step; safe to call at runtime.
    void setSettings(const RandomSettings& settings);

    // Pick an initial target/duration and start. Call after LampController::begin().
    void begin();

    // Advance to a new random target/duration if the current step has elapsed.
    void tick();

    // Lamp mask currently lit; 0 before the first pick.
    uint8_t currentMask() const { return hasCurrent_ ? currentMask_ : 0; }

  private:
    void pickNext();
    uint8_t pickSingleMask() const;
    uint8_t pickMultiMask() const;

    LampController& lamps_;
    RandomSettings settings_;
    uint8_t currentMask_;
    uint32_t stepStartMs_;
    uint32_t stepDurationMs_;
    bool hasCurrent_ = false;
};
