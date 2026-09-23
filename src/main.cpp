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

// Traffic Light Controller - Task 1 skeleton.
// Verifies toolchain, USB flashing, and lamp wiring only.
// Boot-safe init: all lamp GPIOs are driven LOW before anything else, then a
// simple hardcoded sequence confirms each channel is wired to the right color.
// The real non-blocking Sequencer replaces this loop in task 2.

#include <Arduino.h>
#include "config.h"
#include "version.h"

static const int LAMP_GPIOS[] = {RED_GPIO, YELLOW_GPIO, GREEN_GPIO};
static const char *LAMP_NAMES[] = {"RED", "YELLOW", "GREEN"};
static constexpr int LAMP_COUNT = sizeof(LAMP_GPIOS) / sizeof(LAMP_GPIOS[0]);

static void allLampsOff() {
  for (int i = 0; i < LAMP_COUNT; i++) {
    digitalWrite(LAMP_GPIOS[i], LOW);
  }
}

void setup() {
  // Boot-safe lamp init MUST come first (per hardware doc): configure outputs
  // and drive LOW before any other work. Hardware 10k gate pulldowns cover the
  // pre-firmware window; this covers everything after.
  for (int i = 0; i < LAMP_COUNT; i++) {
    pinMode(LAMP_GPIOS[i], OUTPUT);
    digitalWrite(LAMP_GPIOS[i], LOW);
  }

  Serial.begin(SERIAL_BAUD);
  Serial.println();
  Serial.printf("Traffic Light Controller v%s - task 1 skeleton\n", FIRMWARE_VERSION_STRING);
  Serial.printf("Lamp GPIOs: RED=%d YELLOW=%d GREEN=%d\n",
                RED_GPIO, YELLOW_GPIO, GREEN_GPIO);
}

void loop() {
  // Walk each lamp on for 1s to confirm color-to-GPIO wiring.
  for (int i = 0; i < LAMP_COUNT; i++) {
    allLampsOff();
    digitalWrite(LAMP_GPIOS[i], HIGH);
    Serial.printf("%s ON\n", LAMP_NAMES[i]);
    delay(1000);
  }
}
