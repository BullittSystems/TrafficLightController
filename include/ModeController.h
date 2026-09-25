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

// Owns what the lamps show: the persisted mode (the baseline), optionally covered by
// a transient Override. Table-based patterns run on the shared StepMode engine;
// Random runs on its own. Pushes settings into the active pattern and delegates the
// per-loop tick.
class ModeController {
  public:
    ModeController(StepMode& step, RandomMode& random);

    // Apply persisted settings. (Re)starts the mode when it changes; parameter-only
    // changes take effect in place. clearOverride drops any active override (for
    // requests that set the mode); otherwise an override keeps running and the new
    // settings show once it ends.
    void apply(const AppSettings& settings, bool clearOverride = false);

    // Show an override on top of the mode, starting it immediately.
    void setOverride(const Override& override);

    // Return to the mode, restarting it. False if there was no override.
    bool clearOverride();

    // Advance the active pattern and expire a timed override. Call every loop().
    void tick();

    // The persisted settings (the baseline).
    const AppSettings& settings() const { return settings_; }

    // Mode, override, and lit lamps as one consistent snapshot.
    LiveStatus status() const;

    // Lamp mask currently lit, across whichever pattern is active.
    uint8_t currentMask() const;

  private:
    // The settings the lamps follow: the override's if one is active.
    const AppSettings& active() const { return hasOverride_ ? override_.settings : settings_; }
    // Push the active settings into the engines; restart or refresh in place.
    void activate(bool restart);
    // Build the step table for the active pattern and load it into the engine.
    void loadStepTable();
    uint8_t currentMaskLocked() const;

    StepMode& step_;
    RandomMode& random_;
    AppSettings settings_;
    Override override_;
    bool hasOverride_         = false;
    uint32_t overrideStartMs_ = 0;
    bool started_             = false;
    // Serializes state access between the web-server (AsyncTCP) task and the main
    // loop task; both call into the modes concurrently.
    SemaphoreHandle_t mutex_;
};
