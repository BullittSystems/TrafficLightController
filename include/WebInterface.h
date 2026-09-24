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

#include "ModeController.h"
#include "Settings.h"

// Serves the embedded settings page and JSON API on port 80: view and edit the
// operating mode and its per-mode settings, persisting changes and applying them
// to the running modes.
class WebInterface {
  public:
    WebInterface(Settings& settings, ModeController& controller);

    // Register routes and start listening. Call once, after Wi-Fi has connected,
    // so the port is free of the provisioning portal.
    void begin();

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

    AsyncWebServer server_;
    AsyncCallbackJsonWebHandler settingsPostHandler_;
    AsyncCallbackJsonWebHandler statePostHandler_;
    Settings& settings_;
    ModeController& controller_;
};
