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

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "RandomMode.h"
#include "StepMode.h"
#include "config.h"

// Owns the active operating mode. Table-based modes run on the shared StepMode
// engine; Random runs on its own. Pushes settings into the active mode and
// delegates the per-loop tick.
class ModeController {
  public:
    ModeController(StepMode& step, RandomMode& random);

    // Apply settings: refresh each mode's parameters, and (re)start the selected
    // mode when the mode changes. Parameter-only changes take effect on the next
    // step without restarting the active mode.
    void apply(const AppSettings& settings);

    // Apply a transient API state. Returns false (caller responds 409) unless the
    // current mode is Api. On success, drives the lamps immediately.
    bool applyApiState(const ApiState& state);

    // Delegate to the active mode. Call every loop().
    void tick();

    // The settings currently in effect.
    const AppSettings& settings() const { return settings_; }

    // True when the API drives the lamps (mode == Api).
    bool inApiMode() const { return settings_.mode == Mode::Api; }

    // The transient API state currently in effect (meaningful in Api mode).
    const ApiState& apiState() const { return apiState_; }

    // Lamp mask currently lit, across whichever mode is active.
    uint8_t currentMask() const;

  private:
    // Build the step table for the current mode and load it into the engine.
    void loadStepTable();

    StepMode& step_;
    RandomMode& random_;
    AppSettings settings_;
    ApiState apiState_;
    bool started_ = false;
    // Serializes state access between the web-server (AsyncTCP) task and the main
    // loop task; both call into the modes concurrently.
    SemaphoreHandle_t mutex_;
};
