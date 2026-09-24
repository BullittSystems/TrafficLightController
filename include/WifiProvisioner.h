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

#include <WiFiManager.h>

// Manages Wi-Fi provisioning via WiFiManager in non-blocking mode: connects with
// saved credentials, or serves an AP captive portal for setup. Never blocks the
// main loop, so lamp sequencing continues while provisioning is pending.
class WifiProvisioner {
  public:
    // Start provisioning: connect with saved credentials, or open the config
    // portal AP if none are stored / the connection fails.
    void begin();

    // Drive the config portal and connection state machine. Call every loop().
    void process();

  private:
    void logStatusChange();

    WiFiManager wm_;
    bool wasConnected_ = false;
};
