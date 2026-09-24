/*
 * LampController.h
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

#include "config.h"  // Lamp, LAMP_ACTIVE_HIGH

// Drives the three lamp output channels as logical on/off. Output polarity is
// configurable (active-high by default); the load/driver electronics behind the
// pins are the integrator's concern. Digital on/off only; the interface is
// deliberately small so a PWM-backed implementation can replace it later without
// touching callers.
class LampController {
  public:
    explicit LampController(bool activeHigh = LAMP_ACTIVE_HIGH);

    // Boot-safe init: configure all lamp GPIOs as outputs and drive them to the
    // OFF level. Must be called before any other lamp use, as early as practical.
    void begin();

    void set(Lamp lamp, bool on);
    void allOff();

  private:
    bool activeHigh_;
};
