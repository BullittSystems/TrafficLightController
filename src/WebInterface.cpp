/*
 * WebInterface.cpp
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
#include "WebInterface.h"

#include <ArduinoJson.h>

#include "SettingsCodec.h"

namespace {
const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Traffic Light</title>
<style>
  :root { color-scheme: light dark; }
  body { font-family: system-ui, sans-serif; margin: 0; padding: 2rem;
         max-width: 28rem; margin-inline: auto; }
  h1 { font-size: 1.3rem; }
  label { display: flex; justify-content: space-between; align-items: center;
          padding: .6rem 0; }
  fieldset { border: 1px solid #8884; border-radius: .4rem; margin: 1rem 0; }
  legend { padding: 0 .4rem; color: #888; font-size: .85rem; }
  input, select { width: 8rem; padding: .3rem; font-variant-numeric: tabular-nums; }
  button { margin-top: .5rem; padding: .5rem 1rem; }
  .muted { color: #888; font-size: .85rem; }
</style>
</head>
<body>
  <h1>Traffic Light</h1>
  <form id="f">
    <label>Mode
      <select name="mode">
        <option value="api">API</option>
        <option value="solid">Solid</option>
        <option value="blink">Blink</option>
        <option value="sequence">Sequence</option>
        <option value="random">Random</option>
        <option value="off">Off</option>
      </select>
    </label>
    <p id="api" class="muted" hidden>Lamps are driven by POST /api/state while in this mode.</p>
    <fieldset id="solid">
      <legend>Solid</legend>
      <label>Color
        <select name="solidColor">
          <option value="red">Red</option>
          <option value="yellow">Yellow</option>
          <option value="green">Green</option>
        </select>
      </label>
    </fieldset>
    <fieldset id="blink">
      <legend>Blink</legend>
      <label>Color
        <select name="blinkColor">
          <option value="red">Red</option>
          <option value="yellow">Yellow</option>
          <option value="green">Green</option>
        </select>
      </label>
      <label>Interval (ms) <input type="number" name="blinkMs" min="100" max="600000" step="50" required></label>
    </fieldset>
    <fieldset id="seq">
      <legend>Sequence durations (ms)</legend>
      <label>Green <input type="number" name="greenMs" min="100" max="600000" step="50" required></label>
      <label>Yellow <input type="number" name="yellowMs" min="100" max="600000" step="50" required></label>
      <label>Red <input type="number" name="redMs" min="100" max="600000" step="50" required></label>
    </fieldset>
    <fieldset id="rnd">
      <legend>Random hold time (ms)</legend>
      <label>Lamps
        <select name="randomVariant">
          <option value="single">Single</option>
          <option value="multi">Multiple</option>
        </select>
      </label>
      <label>Min <input type="number" name="minMs" min="100" max="600000" step="50" required></label>
      <label>Max <input type="number" name="maxMs" min="100" max="600000" step="50" required></label>
    </fieldset>
    <button type="submit">Save</button>
  </form>
  <p id="status" class="muted">&nbsp;</p>
<script>
  const f = document.getElementById('f');
  const statusEl = document.getElementById('status');
  const fs = {
    api: document.getElementById('api'),
    solid: document.getElementById('solid'),
    blink: document.getElementById('blink'),
    sequence: document.getElementById('seq'),
    random: document.getElementById('rnd')
  };
  function showFor(mode) {
    for (const key in fs) fs[key].hidden = key !== mode;
  }
  function fill(s) {
    f.mode.value = s.mode;
    f.greenMs.value = s.sequence.greenMs;
    f.yellowMs.value = s.sequence.yellowMs;
    f.redMs.value = s.sequence.redMs;
    f.minMs.value = s.random.minMs;
    f.maxMs.value = s.random.maxMs;
    f.randomVariant.value = s.random.variant;
    f.solidColor.value = s.solid.color;
    f.blinkColor.value = s.blink.color;
    f.blinkMs.value = s.blink.intervalMs;
    showFor(s.mode);
  }
  f.mode.addEventListener('change', () => showFor(f.mode.value));
  async function load() {
    fill(await (await fetch('/api/settings')).json());
  }
  f.addEventListener('submit', async (e) => {
    e.preventDefault();
    const body = {
      mode: f.mode.value,
      sequence: { greenMs: +f.greenMs.value, yellowMs: +f.yellowMs.value, redMs: +f.redMs.value },
      random: { minMs: +f.minMs.value, maxMs: +f.maxMs.value, variant: f.randomVariant.value },
      solid: { color: f.solidColor.value },
      blink: { color: f.blinkColor.value, intervalMs: +f.blinkMs.value }
    };
    const r = await fetch('/api/settings', {
      method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body)
    });
    if (r.ok) { fill(await r.json()); statusEl.textContent = 'Saved.'; }
    else { statusEl.textContent = 'Error saving.'; }
  });
  load();
</script>
</body>
</html>)HTML";
}  // namespace

WebInterface::WebInterface(Settings& settings, ModeController& controller)
    : server_(80),
      settingsPostHandler_("/api/settings",
                           [this](AsyncWebServerRequest* request, JsonVariant& json) {
                               handlePostSettings(request, json);
                           }),
      statePostHandler_("/api/state",
                        [this](AsyncWebServerRequest* request, JsonVariant& json) {
                            handlePostState(request, json);
                        }),
      settings_(settings),
      controller_(controller) {}

void WebInterface::begin() {
    server_.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        // ESP32 flash is memory-mapped, so the PROGMEM literal is read directly.
        request->send(200, "text/html", INDEX_HTML);
    });

    // Specific path routes MUST be registered before the generic /api/state and
    // /api/settings GET routes below: the server matches by prefix in registration
    // order, so a generic "/api/state" would otherwise shadow "/api/state/...".

    // Webhook-friendly convenience routes (GET or POST, no body). Blink uses the
    // current/last API interval.
    struct StateRoute {
        const char* path;
        ApiStateKind kind;
        Lamp color;
    };
    static const StateRoute routes[] = {
        {"/api/state/off", ApiStateKind::Off, Lamp::Red},
        {"/api/state/solid/red", ApiStateKind::Solid, Lamp::Red},
        {"/api/state/solid/yellow", ApiStateKind::Solid, Lamp::Yellow},
        {"/api/state/solid/green", ApiStateKind::Solid, Lamp::Green},
        {"/api/state/blink/red", ApiStateKind::Blink, Lamp::Red},
        {"/api/state/blink/yellow", ApiStateKind::Blink, Lamp::Yellow},
        {"/api/state/blink/green", ApiStateKind::Blink, Lamp::Green},
    };
    for (const StateRoute& r : routes) {
        server_.on(r.path, HTTP_GET | HTTP_POST, [this, r](AsyncWebServerRequest* request) {
            logRequest(request);
            ApiState state;
            state.kind       = r.kind;
            state.color      = r.color;
            state.intervalMs = controller_.apiState().intervalMs;
            applyApiCommand(request, state);
        });
    }

    // Random variant convenience routes (persisted). MUST be registered before the
    // generic /api/mode/random below, which prefix-matches these.
    server_.on("/api/mode/random/single", HTTP_GET | HTTP_POST,
               [this](AsyncWebServerRequest* request) {
                   logRequest(request);
                   applyRandomVariant(request, RandomVariant::Single);
               });
    server_.on("/api/mode/random/multi", HTTP_GET | HTTP_POST,
               [this](AsyncWebServerRequest* request) {
                   logRequest(request);
                   applyRandomVariant(request, RandomVariant::Multi);
               });

    // Webhook-friendly persisted mode switches (GET or POST). Each activates the
    // mode using its already-saved parameters.
    struct ModeRoute {
        const char* path;
        Mode mode;
    };
    static const ModeRoute modeRoutes[] = {
        {"/api/mode/api", Mode::Api},       {"/api/mode/solid", Mode::Solid},
        {"/api/mode/blink", Mode::Blink},   {"/api/mode/sequence", Mode::Sequence},
        {"/api/mode/random", Mode::Random}, {"/api/mode/off", Mode::Off},
    };
    for (const ModeRoute& r : modeRoutes) {
        server_.on(r.path, HTTP_GET | HTTP_POST, [this, r](AsyncWebServerRequest* request) {
            logRequest(request);
            applyModeChange(request, r.mode);
        });
    }

    // Generic reads, registered after the specific path routes (see note above).
    server_.on("/api/settings", HTTP_GET, [this](AsyncWebServerRequest* request) {
        logRequest(request);
        handleGetSettings(request);
    });
    server_.on("/api/state", HTTP_GET, [this](AsyncWebServerRequest* request) {
        logRequest(request);
        handleGetState(request);
    });

    settingsPostHandler_.setMethod(HTTP_POST);
    server_.addHandler(&settingsPostHandler_);
    statePostHandler_.setMethod(HTTP_POST);
    server_.addHandler(&statePostHandler_);

    server_.begin();
}

void WebInterface::handleGetSettings(AsyncWebServerRequest* request) {
    JsonDocument doc;
    SettingsCodec::writeSettings(doc, controller_.settings());

    String body;
    serializeJson(doc, body);
    request->send(200, "application/json", body);
}

void WebInterface::handlePostSettings(AsyncWebServerRequest* request, JsonVariant& json) {
    logRequest(request);
    if (!json.is<JsonObject>()) {
        request->send(400, "application/json", "{\"error\":\"expected JSON object\"}");
        return;
    }

    // Start from current settings so absent fields are left unchanged.
    AppSettings app = controller_.settings();
    const SettingsCodec::Result parse =
        SettingsCodec::applySettingsPatch(json.as<JsonObjectConst>(), app);
    if (!parse.ok) {
        String err = String("{\"error\":\"") + parse.error + "\"}";
        request->send(400, "application/json", err);
        return;
    }

    settings_.save(app);
    controller_.apply(app);

    JsonDocument doc;
    SettingsCodec::writeSettings(doc, controller_.settings());
    String body;
    serializeJson(doc, body);
    request->send(200, "application/json", body);
}

void WebInterface::handleGetState(AsyncWebServerRequest* request) {
    JsonDocument doc;
    SettingsCodec::writeState(doc, controller_.settings().mode, controller_.currentMask(),
                              controller_.inApiMode(), controller_.apiState());

    String body;
    serializeJson(doc, body);
    request->send(200, "application/json", body);
}

void WebInterface::handlePostState(AsyncWebServerRequest* request, JsonVariant& json) {
    logRequest(request);
    if (!json.is<JsonObject>()) {
        request->send(400, "application/json", "{\"error\":\"expected JSON object\"}");
        return;
    }

    ApiState state;
    state.intervalMs = controller_.apiState().intervalMs;  // default to current
    const SettingsCodec::Result parse =
        SettingsCodec::parseApiCommand(json.as<JsonObjectConst>(), state);
    if (!parse.ok) {
        String err = String("{\"error\":\"") + parse.error + "\"}";
        request->send(400, "application/json", err);
        return;
    }

    applyApiCommand(request, state);
}

void WebInterface::applyApiCommand(AsyncWebServerRequest* request, const ApiState& state) {
    if (!controller_.applyApiState(state)) {
        Serial.println("HTTP: /api/state rejected (not in API mode)");
        request->send(409, "application/json", "{\"error\":\"not in API mode\"}");
        return;
    }
    handleGetState(request);  // report the resulting state
}

void WebInterface::logRequest(AsyncWebServerRequest* request) {
    Serial.printf("HTTP: %s %s from %s\n", request->methodToString(), request->url().c_str(),
                  request->client()->remoteIP().toString().c_str());
}

void WebInterface::applyModeChange(AsyncWebServerRequest* request, Mode mode) {
    AppSettings app = controller_.settings();
    app.mode        = mode;
    settings_.save(app);
    controller_.apply(app);
    handleGetState(request);  // report the resulting state
}

void WebInterface::applyRandomVariant(AsyncWebServerRequest* request, RandomVariant variant) {
    AppSettings app    = controller_.settings();
    app.mode           = Mode::Random;
    app.random.variant = variant;
    settings_.save(app);
    controller_.apply(app);
    handleGetState(request);  // report the resulting state
}
