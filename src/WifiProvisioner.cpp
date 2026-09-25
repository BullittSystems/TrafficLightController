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
    // Fully non-blocking bring-up: never call the blocking autoConnect(), which
    // stalls setup() (and therefore the lamps) in waitForConnectResult() when a
    // saved network is present but slow/failing. Instead kick off an async STA
    // connect here and drive everything from process() so loop() runs immediately.
    wm_.setConfigPortalBlocking(false);
    WiFi.mode(WIFI_STA);

    if (wm_.getWiFiIsSaved()) {
        WiFi.begin();  // async; uses persisted credentials
        staStartMs_ = millis();
        Serial.println("WiFi: connecting with saved credentials...");
    } else {
        startPortal();
    }
}

void WifiProvisioner::process() {
    if (portalStarted_) {
        wm_.process();
    } else if (WiFi.status() != WL_CONNECTED &&
               millis() - staStartMs_ >= WIFI_STA_CONNECT_TIMEOUT_MS) {
        Serial.println("WiFi: saved-credential connect timed out");
        startPortal();
    }
    logStatusChange();
}

void WifiProvisioner::startPortal() {
    // Non-blocking (setConfigPortalBlocking(false)); process() services it.
    wm_.startConfigPortal(AP_SSID);
    portalStarted_ = true;
    Serial.printf("WiFi: config portal '%s' active (connect to provision)\n", AP_SSID);
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
