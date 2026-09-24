/*
 * main.cpp
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

// Traffic Light Controller entry point. Boot-safe lamp init runs first, then the
// non-blocking Sequencer drives the standard traffic-light cycle.

#include <Arduino.h>

#include "LampController.h"
#include "Sequencer.h"
#include "Settings.h"
#include "config.h"
#include "version.h"

static LampController lamps;
static Sequencer sequencer(lamps);
static Settings settings;

void setup() {
    // Boot-safe lamp init MUST come first (per hardware doc): configure outputs
    // and drive LOW before any other work. Hardware 10k gate pulldowns cover the
    // pre-firmware window; this covers everything after.
    lamps.begin();

    Serial.begin(SERIAL_BAUD);
    Serial.println();
    Serial.printf("Traffic Light Controller v%s\n", FIRMWARE_VERSION_STRING);
    Serial.printf("Lamp GPIOs: RED=%d YELLOW=%d GREEN=%d\n", RED_GPIO, YELLOW_GPIO, GREEN_GPIO);

    // Load persisted config (defaults on first boot) and start sequencing from it.
    PhaseDurations durations = settings.load();
    Serial.printf("Phase durations (ms): green=%lu yellow=%lu red=%lu\n",
                  (unsigned long) durations.greenMs, (unsigned long) durations.yellowMs,
                  (unsigned long) durations.redMs);

    sequencer.setDurations(durations);
    sequencer.begin();
}

void loop() {
    sequencer.tick();
}
