/*
 * WebInterface.h
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

#include <AsyncJson.h>
#include <ESPAsyncWebServer.h>

#include <atomic>

#include "ModeController.h"
#include "Settings.h"
#include "WifiProvisioner.h"

// Serves the embedded settings page and JSON API on port 80: view and edit the
// operating mode and its per-mode settings, persisting changes and applying them
// to the running modes. Also manages the saved Wi-Fi networks.
class WebInterface {
  public:
    WebInterface(Settings& settings, ModeController& controller, WifiProvisioner& wifi);

    // Register routes and start listening. Call once, after Wi-Fi has connected,
    // so the port is free of the provisioning portal.
    void begin();

    // True once POST /api/wifi/reset was accepted. The reset and restart run from
    // loop(), after the response has gone out, not on the AsyncTCP task.
    bool wifiResetRequested() const { return wifiResetRequested_; }

  private:
    void handleGetSettings(AsyncWebServerRequest* request);
    void handlePostSettings(AsyncWebServerRequest* request, JsonVariant& json);

    // GET /api/state: report current mode, live lit lamp, and API state.
    void handleGetState(AsyncWebServerRequest* request);
    // POST /api/state (JSON body): set the transient API state.
    void handlePostState(AsyncWebServerRequest* request, JsonVariant& json);
    // Apply an API command: 409 unless in API mode, otherwise apply and report state.
    void applyApiCommand(AsyncWebServerRequest* request, const ApiState& state);
    // Switch the top-level mode (persisted) and report the resulting state.
    void applyModeChange(AsyncWebServerRequest* request, Mode mode);
    // Switch to Random mode with a specific variant (persisted) and report state.
    void applyRandomVariant(AsyncWebServerRequest* request, RandomVariant variant);
    // Log an incoming API request (method, path, client IP) to the serial console.
    void logRequest(AsyncWebServerRequest* request);

    // GET /api/wifi, and the response body of the other /api/wifi routes.
    void handleGetWifi(AsyncWebServerRequest* request);
    void handlePostWifiAdd(AsyncWebServerRequest* request, JsonVariant& json);
    void handlePostWifiForget(AsyncWebServerRequest* request, JsonVariant& json);
    void handlePostWifiKeep(AsyncWebServerRequest* request, JsonVariant& json);
    static void sendError(AsyncWebServerRequest* request, int code, const char* error);

    AsyncWebServer server_;
    AsyncCallbackJsonWebHandler settingsPostHandler_;
    AsyncCallbackJsonWebHandler statePostHandler_;
    AsyncCallbackJsonWebHandler wifiAddHandler_;
    AsyncCallbackJsonWebHandler wifiForgetHandler_;
    AsyncCallbackJsonWebHandler wifiKeepHandler_;
    Settings& settings_;
    ModeController& controller_;
    WifiProvisioner& wifi_;
    std::atomic<bool> wifiResetRequested_{false};
};
