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

ModeController::ModeController(StepMode& step, RandomMode& random)
    : step_(step), random_(random), mutex_(xSemaphoreCreateMutex()) {}

void ModeController::apply(const AppSettings& settings) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    const bool modeChanged = !started_ || settings.mode != settings_.mode;
    settings_              = settings;

    // Entering API mode shows a "ready" indicator (yellow blink) until the first
    // /api/state command, so a powered, waiting device never looks dead/off.
    if (modeChanged && settings_.mode == Mode::Api) {
        apiState_       = ApiState{};
        apiState_.kind  = ApiStateKind::Blink;
        apiState_.color = Lamp::Yellow;
        // intervalMs keeps its default (DEFAULT_BLINK_MS).
    }

    random_.setSettings(settings_.random);
    loadStepTable();  // refreshes parameters; no restart on its own

    // Restart only on an actual mode change (or first apply). Parameter-only edits
    // re-apply in place (refresh) so changes show immediately without restarting
    // the cadence.
    if (modeChanged) {
        if (settings_.mode == Mode::Random) {
            random_.begin();
        } else {
            step_.begin();
        }
        started_ = true;
    } else if (settings_.mode != Mode::Random) {
        step_.refresh();
    }
    xSemaphoreGive(mutex_);
}

bool ModeController::applyApiState(const ApiState& state) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    if (settings_.mode != Mode::Api) {
        xSemaphoreGive(mutex_);
        return false;
    }
    apiState_ = state;
    loadStepTable();
    step_.begin();
    xSemaphoreGive(mutex_);
    return true;
}

void ModeController::tick() {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    if (settings_.mode == Mode::Random) {
        random_.tick();
    } else {
        step_.tick();
    }
    xSemaphoreGive(mutex_);
}

uint8_t ModeController::currentMask() const {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    const uint8_t mask =
        settings_.mode == Mode::Random ? random_.currentMask() : step_.currentMask();
    xSemaphoreGive(mutex_);
    return mask;
}

void ModeController::loadStepTable() {
    Step steps[MAX_STEPS];
    uint8_t count = 0;

    switch (settings_.mode) {
        case Mode::Api:
            switch (apiState_.kind) {
                case ApiStateKind::Solid:
                    steps[0] = {lampBit(apiState_.color), 0};
                    count    = 1;
                    break;
                case ApiStateKind::Blink:
                    steps[0] = {lampBit(apiState_.color), apiState_.intervalMs};
                    steps[1] = {0, apiState_.intervalMs};
                    count    = 2;
                    break;
                case ApiStateKind::Off:
                    steps[0] = {0, 0};
                    count    = 1;
                    break;
            }
            break;
        case Mode::Solid:
            steps[0] = {lampBit(settings_.solid.color), 0};
            count    = 1;
            break;
        case Mode::Blink:
            steps[0] = {lampBit(settings_.blink.color), settings_.blink.intervalMs};
            steps[1] = {0, settings_.blink.intervalMs};
            count    = 2;
            break;
        case Mode::Sequence: {
            const PhaseDurations& d = settings_.sequence;
            steps[0]                = {lampBit(Lamp::Green), d.greenMs};
            steps[1]                = {lampBit(Lamp::Yellow), d.yellowMs};
            steps[2]                = {lampBit(Lamp::Red), d.redMs};
            count                   = 3;
            break;
        }
        case Mode::Random:
            return;  // Random does not use the step engine
        case Mode::Off:
            steps[0] = {0, 0};
            count    = 1;
            break;
    }

    step_.setSteps(steps, count);
}
