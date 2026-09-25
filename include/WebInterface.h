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

    // Register routes (first call only) and start listening. Call after Wi-Fi has
    // connected. Safe to call again if listening() is still false: after a portal
    // session, port 80 can stay held by the phone's connection in TCP TIME_WAIT
    // (up to ~2 min), and the bind fails until it clears.
    void begin();

    // True once the server is bound and listening on port 80.
    bool listening() const { return server_.state() == LISTEN; }

    // True once POST /api/wifi/reset was accepted. The reset and restart run from
    // loop(), after the response has gone out, not on the AsyncTCP task.
    bool wifiResetRequested() const { return wifiResetRequested_; }

  private:
    void registerRoutes();
    void handleGetSettings(AsyncWebServerRequest* request);
    void handlePostSettings(AsyncWebServerRequest* request, JsonVariant& json);

    // GET /api/state: persisted mode, what is showing, and the override.
    void handleGetState(AsyncWebServerRequest* request);
    // POST /api/override (JSON body): show a transient pattern over the mode.
    void handlePostOverride(AsyncWebServerRequest* request, JsonVariant& json);
    // /api/mode/<pattern>?params: persist the pattern and params as the mode, and
    // clear any override.
    void applyModeRoute(AsyncWebServerRequest* request, Mode pattern);
    // /api/override/<pattern>?params[&ttlMs=]: show the pattern transiently.
    void applyOverrideRoute(AsyncWebServerRequest* request, Mode pattern);
    // Build the parser input for a one-URL route: the query parameters plus the
    // pattern from the path. False (400 sent) if the query tries to set the pattern.
    bool queryCommand(AsyncWebServerRequest* request, Mode pattern, JsonDocument& doc);
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
    AsyncCallbackJsonWebHandler overridePostHandler_;
    AsyncCallbackJsonWebHandler wifiAddHandler_;
    AsyncCallbackJsonWebHandler wifiForgetHandler_;
    AsyncCallbackJsonWebHandler wifiKeepHandler_;
    Settings& settings_;
    ModeController& controller_;
    WifiProvisioner& wifi_;
    std::atomic<bool> wifiResetRequested_{false};
    bool routesRegistered_ = false;
};
