/*
 * WifiProvisioner.cpp
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
#include "WifiProvisioner.h"

#include <Arduino.h>
#include <WiFi.h>

#include "config.h"

void WifiProvisioner::begin() {
    // Non-blocking: autoConnect returns immediately. With saved credentials it
    // connects; otherwise it opens the config portal AP and returns false, and
    // process() drives the portal from the main loop without stalling sequencing.
    wm_.setConfigPortalBlocking(false);

    if (wm_.autoConnect(AP_SSID)) {
        wasConnected_ = true;
        Serial.printf("WiFi: connected, IP=%s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.printf("WiFi: config portal '%s' active (connect to provision)\n", AP_SSID);
    }
}

void WifiProvisioner::process() {
    wm_.process();
    logStatusChange();
}

bool WifiProvisioner::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

void WifiProvisioner::resetCredentials() {
    wm_.resetSettings();
}

void WifiProvisioner::logStatusChange() {
    const bool connected = (WiFi.status() == WL_CONNECTED);
    if (connected && !wasConnected_) {
        Serial.printf("WiFi: connected, IP=%s\n", WiFi.localIP().toString().c_str());
    } else if (!connected && wasConnected_) {
        Serial.println("WiFi: connection lost");
    }
    wasConnected_ = connected;
}
