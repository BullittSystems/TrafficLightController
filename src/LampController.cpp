/*
 * LampController.cpp
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
#include "LampController.h"

#include <Arduino.h>

#include "config.h"

namespace {
int pinFor(Lamp lamp) {
    switch (lamp) {
        case Lamp::Red:
            return RED_GPIO;
        case Lamp::Yellow:
            return YELLOW_GPIO;
        case Lamp::Green:
            return GREEN_GPIO;
    }
    return -1;  // unreachable; enum is exhaustively handled above
}
}  // namespace

void LampController::begin() {
    pinMode(RED_GPIO, OUTPUT);
    pinMode(YELLOW_GPIO, OUTPUT);
    pinMode(GREEN_GPIO, OUTPUT);
    allOff();
}

void LampController::set(Lamp lamp, bool on) {
    digitalWrite(pinFor(lamp), on ? HIGH : LOW);
}

void LampController::allOff() {
    set(Lamp::Red, false);
    set(Lamp::Yellow, false);
    set(Lamp::Green, false);
}
