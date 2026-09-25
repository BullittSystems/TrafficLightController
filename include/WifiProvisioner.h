/*
 * WifiProvisioner.h
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

#include "KnownNetworks.h"

// Connects to the strongest in-range network from a saved list, falling back to a
// WiFiManager captive portal when none are reachable. Networks provisioned through
// the portal are appended to the list; if the list is full, the new network is kept
// as a RAM-only "session" network until reboot (the user decides what to keep via
// the API). Fully non-blocking (async scan, async connect), so lamp sequencing never
// waits on Wi-Fi.
//
// The list and session network are guarded by a mutex: process() runs on the loop()
// task while the list-editing methods are called from web handlers (AsyncTCP task).
class WifiProvisioner {
  public:
    enum class KeepResult : uint8_t { Kept, NoSession, Full, NotFound, Invalid };

    WifiProvisioner();

    // Load the saved list and start the first scan (or the portal if the list is empty).
    void begin();

    // Drive the scan / connect / portal state machine. Call every loop().
    void process();

    // True when connected to an access point (STA).
    bool isConnected() const;

    // Forget every saved network and any session network. The next boot has none,
    // so provisioning opens.
    void resetCredentials();

    // Make the next boot open the portal instead of connecting, keeping the saved
    // list. Survives a software restart only (RTC memory, no flash write). The
    // caller restarts: the portal can't open in place while the web server holds
    // port 80.
    static void requestPortalOnBoot();

    // Saved list, session network, and current connection, without passwords.
    WifiSnapshot snapshot() const;

    // Save a network (see KnownNetworks::add). Saving the session network's SSID
    // persists it and ends the session entry. Takes effect on the next scan.
    KnownNetworks::AddResult addNetwork(const char* ssid, const char* password);

    // Remove a saved network. An active connection to it stays up until it drops.
    bool forgetNetwork(const char* ssid);

    // Persist the session network: appended if there is room (replaceSsid null),
    // otherwise written over replaceSsid in place.
    KeepResult keepSessionNetwork(const char* replaceSsid);

  private:
    enum class State : uint8_t { Scanning, Connecting, Connected, Portal };

    void enter(State state);
    void startScanOrPortal();
    void connectToBestScanResult(int16_t found);
    const KnownNetwork* credentialsFor(const char* ssid) const;
    void startPortal();
    void captureProvisionedNetwork();
    void logStatusChange();

    SemaphoreHandle_t mutex_;
    KnownNetworks networks_;
    KnownNetwork session_    = {};
    bool hasSession_         = false;
    State state_             = State::Scanning;
    uint32_t stateStartMs_   = 0;
    uint32_t lostSinceMs_    = 0;
    char connectingSsid_[33] = {};
    bool wasConnected_       = false;
};
