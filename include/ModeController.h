/*
 * ModeController.h
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

#include "RandomMode.h"
#include "SequenceMode.h"
#include "config.h"

// Owns the active operating mode. Pushes settings into each mode and delegates
// the per-loop tick to whichever mode is selected.
class ModeController {
  public:
    ModeController(SequenceMode& sequence, RandomMode& random);

    // Apply settings: update each mode's parameters, and (re)start the selected
    // mode when the mode changes. Parameter-only changes take effect on the next
    // step without restarting the active mode.
    void apply(const AppSettings& settings);

    // Delegate to the active mode. Call every loop().
    void tick();

    // The settings currently in effect.
    const AppSettings& settings() const { return settings_; }

  private:
    SequenceMode& sequence_;
    RandomMode& random_;
    AppSettings settings_;
    bool started_ = false;
};
