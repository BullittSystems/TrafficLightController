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
#include "ModeController.h"
#include "RandomMode.h"
#include "Settings.h"
#include "StepMode.h"
#include "WebInterface.h"
#include "WifiProvisioner.h"
#include "config.h"
#include "version.h"

static LampController lamps;
static StepMode stepMode(lamps);
static RandomMode randomMode(lamps);
static ModeController controller(stepMode, randomMode);
static Settings settings;
static WifiProvisioner wifi;
static WebInterface web(settings, controller);
static bool webStarted = false;

void setup() {
    // Boot-safe lamp init MUST come first: configure outputs and drive them to the
    // OFF level before any other work. The integrator's wiring should hold the OFF
    // level pre-firmware (the reference build uses 10k gate pulldowns); begin()
    // covers everything after.
    lamps.begin();

    Serial.begin(SERIAL_BAUD);
    Serial.println();
    Serial.printf("Traffic Light Controller v%s\n", FIRMWARE_VERSION_STRING);
    Serial.printf("Lamp GPIOs: RED=%d YELLOW=%d GREEN=%d\n", RED_GPIO, YELLOW_GPIO, GREEN_GPIO);

    // Load persisted settings (defaults on first boot) and start the active mode.
    AppSettings app = settings.load();
    Serial.printf("Mode: %s\n", app.mode == Mode::Random ? "random" : "sequence");
    Serial.printf("Sequence (ms): green=%lu yellow=%lu red=%lu\n",
                  (unsigned long) app.sequence.greenMs, (unsigned long) app.sequence.yellowMs,
                  (unsigned long) app.sequence.redMs);
    Serial.printf("Random bounds (ms): min=%lu max=%lu\n", (unsigned long) app.random.minMs,
                  (unsigned long) app.random.maxMs);
    Serial.printf("Blink interval (ms): %lu\n", (unsigned long) app.blink.intervalMs);

    controller.apply(app);

    // Bring up Wi-Fi in the background. Lamp behavior never depends on it.
    wifi.begin();
}

void loop() {
    controller.tick();
    wifi.process();

    // Start the web server once, after Wi-Fi connects, so it does not contend
    // with the provisioning portal for port 80.
    if (!webStarted && wifi.isConnected()) {
        web.begin();
        webStarted = true;
        Serial.println("Web UI: started");
    }
}
