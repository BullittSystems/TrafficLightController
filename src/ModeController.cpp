/*
 * ModeController.cpp
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
#include "ModeController.h"

#include <Arduino.h>

ModeController::ModeController(StepMode& step, RandomMode& random)
    : step_(step), random_(random), mutex_(xSemaphoreCreateMutex()) {}

void ModeController::apply(const AppSettings& settings, bool clearOverride) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    // Restart only on an actual mode change (or first apply). Parameter-only edits
    // re-apply in place (refresh) so changes show immediately without restarting
    // the cadence.
    bool restart = !started_ || settings.mode != settings_.mode;
    settings_    = settings;
    if (clearOverride && hasOverride_) {
        hasOverride_ = false;
        restart      = true;
    }
    if (!hasOverride_) {
        activate(restart);
    }
    xSemaphoreGive(mutex_);
}

void ModeController::setOverride(const Override& override) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    override_        = override;
    hasOverride_     = true;
    overrideStartMs_ = millis();
    activate(/*restart=*/true);
    xSemaphoreGive(mutex_);
}

bool ModeController::clearOverride() {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    const bool had = hasOverride_;
    if (had) {
        hasOverride_ = false;
        activate(/*restart=*/true);
    }
    xSemaphoreGive(mutex_);
    return had;
}

void ModeController::tick() {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    // Unsigned subtraction is rollover-safe across the millis() wrap.
    if (hasOverride_ && override_.ttlMs != 0 && millis() - overrideStartMs_ >= override_.ttlMs) {
        hasOverride_ = false;
        activate(/*restart=*/true);
    }
    if (active().mode == Mode::Random) {
        random_.tick();
    } else {
        step_.tick();
    }
    xSemaphoreGive(mutex_);
}

LiveStatus ModeController::status() const {
    LiveStatus s;
    xSemaphoreTake(mutex_, portMAX_DELAY);
    s.mode       = settings_.mode;
    s.litMask    = currentMaskLocked();
    s.overridden = hasOverride_;
    if (hasOverride_) {
        s.active = override_;
        if (override_.ttlMs != 0) {
            const uint32_t elapsed = millis() - overrideStartMs_;
            s.remainingMs          = elapsed < override_.ttlMs ? override_.ttlMs - elapsed : 0;
        }
    }
    xSemaphoreGive(mutex_);
    return s;
}

uint8_t ModeController::currentMask() const {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    const uint8_t mask = currentMaskLocked();
    xSemaphoreGive(mutex_);
    return mask;
}

uint8_t ModeController::currentMaskLocked() const {
    return active().mode == Mode::Random ? random_.currentMask() : step_.currentMask();
}

void ModeController::activate(bool restart) {
    const AppSettings& a = active();
    random_.setSettings(a.random);
    loadStepTable();  // refreshes parameters; no restart on its own
    if (restart) {
        if (a.mode == Mode::Random) {
            random_.begin();
        } else {
            step_.begin();
        }
        started_ = true;
    } else if (a.mode != Mode::Random) {
        step_.refresh();
    }
}

void ModeController::loadStepTable() {
    const AppSettings& a = active();
    Step steps[MAX_STEPS];
    uint8_t count = 0;

    switch (a.mode) {
        case Mode::Solid:
            steps[0] = {lampBit(a.solid.color), 0};
            count    = 1;
            break;
        case Mode::Blink:
            steps[0] = {lampBit(a.blink.color), a.blink.intervalMs};
            steps[1] = {0, a.blink.intervalMs};
            count    = 2;
            break;
        case Mode::Sequence: {
            const PhaseDurations& d = a.sequence;
            steps[0]                = {lampBit(Lamp::Green), d.greenMs};
            steps[1]                = {lampBit(Lamp::Yellow), d.yellowMs};
            steps[2]                = {lampBit(Lamp::Red), d.redMs};
            count                   = 3;
            break;
        }
        case Mode::Random:
            return;  // Random does not use the step engine
        case Mode::Chase: {
            // Walk R -> R+Y -> Y -> Y+G -> G -> G+R, overlapping into the next color.
            const uint32_t iv = a.chase.intervalMs;
            const uint8_t r   = lampBit(Lamp::Red);
            const uint8_t y   = lampBit(Lamp::Yellow);
            const uint8_t g   = lampBit(Lamp::Green);
            steps[0]          = {r, iv};
            steps[1]          = {static_cast<uint8_t>(r | y), iv};
            steps[2]          = {y, iv};
            steps[3]          = {static_cast<uint8_t>(y | g), iv};
            steps[4]          = {g, iv};
            steps[5]          = {static_cast<uint8_t>(g | r), iv};
            count             = 6;
            break;
        }
        case Mode::Off:
            steps[0] = {0, 0};
            count    = 1;
            break;
    }

    step_.setSteps(steps, count);
}
