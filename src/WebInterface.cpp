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
#include "WifiCodec.h"

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
  h2 { font-size: 1.1rem; margin-top: 2rem; }
  ul { list-style: none; padding: 0; margin: 0; }
  li { display: flex; justify-content: space-between; align-items: center; padding: .3rem 0; }
  li button { margin: 0; padding: .2rem .6rem; }
  [hidden] { display: none !important; }
</style>
</head>
<body>
  <h1>Traffic Light</h1>
  <form id="f">
    <label>Mode
      <select name="mode">
        <option value="solid">Solid</option>
        <option value="blink">Blink</option>
        <option value="sequence">Sequence</option>
        <option value="random">Random</option>
        <option value="chase">Chase</option>
        <option value="off">Off</option>
      </select>
    </label>
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
    <fieldset id="chase">
      <legend>Chase step (ms)</legend>
      <label>Interval (ms) <input type="number" name="chaseMs" min="100" max="600000" step="50" required></label>
    </fieldset>
    <button type="submit">Save</button>
  </form>
  <p id="status" class="muted">&nbsp;</p>
  <p id="ovr" class="muted" hidden>Override: <span id="ovrText"></span>
    <button id="ovrClear" type="button">Clear</button></p>

  <h2>Wi-Fi</h2>
  <p id="wifiNow" class="muted">&nbsp;</p>
  <fieldset>
    <legend>Saved networks (<span id="netCount">0</span>/<span id="netMax">5</span>)</legend>
    <ul id="nets"></ul>
  </fieldset>
  <fieldset id="session" hidden>
    <legend>Temporary network (forgotten on reboot)</legend>
    <p id="sessionSsid"></p>
    <label id="replaceLabel">Replace <select id="replace"></select></label>
    <button id="keep" type="button">Keep</button>
  </fieldset>
  <form id="addNet">
    <fieldset>
      <legend>Add network</legend>
      <label>SSID <input name="ssid" maxlength="32" required></label>
      <label>Password <input name="password" type="password" maxlength="64"></label>
      <button type="submit">Add</button>
    </fieldset>
  </form>
  <button id="wifiReset" type="button">Forget all networks and restart</button>
  <p id="wifiStatus" class="muted">&nbsp;</p>
<script>
  const f = document.getElementById('f');
  const statusEl = document.getElementById('status');
  const fs = {
    solid: document.getElementById('solid'),
    blink: document.getElementById('blink'),
    sequence: document.getElementById('seq'),
    random: document.getElementById('rnd'),
    chase: document.getElementById('chase')
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
    f.chaseMs.value = s.chase.intervalMs;
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
      blink: { color: f.blinkColor.value, intervalMs: +f.blinkMs.value },
      chase: { intervalMs: +f.chaseMs.value }
    };
    const r = await fetch('/api/settings', {
      method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body)
    });
    if (r.ok) { fill(await r.json()); statusEl.textContent = 'Saved.'; loadState(); }
    else { statusEl.textContent = 'Error saving.'; }
  });
  load();

  // Wi-Fi. SSIDs come from nearby APs, so render them as text, never as HTML.
  const $ = (id) => document.getElementById(id);
  const wifiStatus = $('wifiStatus');
  let wifi = null;
  function forgetButton(ssid) {
    const b = document.createElement('button');
    b.type = 'button';
    b.textContent = 'Forget';
    b.onclick = () => confirm(`Forget "${ssid}"?`) && wifiPost('/api/wifi/forget', { ssid });
    return b;
  }
  function renderWifi(w) {
    wifi = w;
    $('wifiNow').textContent = w.connected ? `Connected to ${w.connected}` : 'Not connected';
    $('netCount').textContent = w.networks.length;
    $('netMax').textContent = w.max;
    $('nets').replaceChildren(...w.networks.map((n) => {
      const li = document.createElement('li');
      const name = document.createElement('span');
      name.textContent = n.ssid + (n.ssid === w.connected ? ' (connected)' : '');
      li.append(name, forgetButton(n.ssid));
      return li;
    }));
    $('session').hidden = !w.session;
    if (w.session) {
      $('sessionSsid').textContent = w.session;
      $('replaceLabel').hidden = w.networks.length < w.max;
      $('replace').replaceChildren(...w.networks.map((n) => new Option(n.ssid, n.ssid)));
    }
  }
  async function wifiPost(path, body) {
    const r = await fetch(path, {
      method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body)
    });
    const j = await r.json().catch(() => ({}));
    if (r.ok && j.networks) renderWifi(j);
    wifiStatus.textContent = r.ok ? 'Saved.' : (j.error || 'Error.');
    return r.ok;
  }
  $('keep').onclick = () => {
    const full = wifi.networks.length >= wifi.max;
    wifiPost('/api/wifi/keep', full ? { replace: $('replace').value } : {});
  };
  $('addNet').addEventListener('submit', async (e) => {
    e.preventDefault();
    const a = e.target;
    if (await wifiPost('/api/wifi/add', { ssid: a.ssid.value, password: a.password.value })) {
      a.reset();
    }
  });
  $('wifiReset').onclick = async () => {
    if (!confirm('Forget every saved network and restart into the setup portal?')) return;
    const r = await fetch('/api/wifi/reset', { method: 'POST' });
    wifiStatus.textContent = r.ok ? 'Restarting into the TrafficLight-Setup portal...' : 'Error.';
  };
  fetch('/api/wifi').then((r) => r.json()).then(renderWifi);

  // Override: a transient pattern over the mode, set via /api/override.
  async function loadState() {
    const o = (await (await fetch('/api/state')).json()).override;
    $('ovr').hidden = !o;
    if (!o) return;
    let text = o.pattern + (o.color ? ` ${o.color}` : '');
    if (o.remainingMs !== undefined) text += ` (${Math.ceil(o.remainingMs / 1000)} s left)`;
    $('ovrText').textContent = text;
  }
  $('ovrClear').onclick = async () => {
    await fetch('/api/override/clear', { method: 'POST' });
    loadState();
  };
  loadState();
</script>
</body>
</html>)HTML";
}  // namespace

WebInterface::WebInterface(Settings& settings, ModeController& controller, WifiProvisioner& wifi)
    : server_(80),
      settingsPostHandler_("/api/settings",
                           [this](AsyncWebServerRequest* request, JsonVariant& json) {
                               handlePostSettings(request, json);
                           }),
      overridePostHandler_("/api/override",
                           [this](AsyncWebServerRequest* request, JsonVariant& json) {
                               handlePostOverride(request, json);
                           }),
      wifiAddHandler_("/api/wifi/add",
                      [this](AsyncWebServerRequest* request, JsonVariant& json) {
                          handlePostWifiAdd(request, json);
                      }),
      wifiForgetHandler_("/api/wifi/forget",
                         [this](AsyncWebServerRequest* request, JsonVariant& json) {
                             handlePostWifiForget(request, json);
                         }),
      wifiKeepHandler_("/api/wifi/keep",
                       [this](AsyncWebServerRequest* request, JsonVariant& json) {
                           handlePostWifiKeep(request, json);
                       }),
      settings_(settings),
      controller_(controller),
      wifi_(wifi) {}

void WebInterface::begin() {
    if (!routesRegistered_) {
        registerRoutes();
        routesRegistered_ = true;
    }
    server_.begin();  // no-op once listening; retries the bind otherwise
}

void WebInterface::registerRoutes() {
    server_.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        // ESP32 flash is memory-mapped, so the PROGMEM literal is read directly.
        request->send(200, "text/html", INDEX_HTML);
    });

    // Generic routes below are prefix-style ("/api/wifi" also matches "/api/wifi/x"),
    // so specific routes MUST be registered first; e.g. the JSON POST /api/override
    // handler would otherwise catch "/api/override/...". The pattern routes use
    // exact matching, so "/api/mode/blink/red" is a 404, not blink.

    // Webhook-friendly one-URL routes (GET or POST, no body): the path names the
    // pattern, the query carries its parameters (?color=red&intervalMs=200). Served
    // under both /api/mode (persisted) and /api/override (transient, plus ttlMs).
    static const Mode patterns[] = {Mode::Solid,  Mode::Blink, Mode::Sequence,
                                    Mode::Random, Mode::Chase, Mode::Off};
    for (const Mode pattern : patterns) {
        const char* name = SettingsCodec::modeName(pattern);
        server_.on(AsyncURIMatcher::exact(String("/api/mode/") + name), HTTP_GET | HTTP_POST,
                   [this, pattern](AsyncWebServerRequest* request) {
                       logRequest(request);
                       applyModeRoute(request, pattern);
                   });
        server_.on(AsyncURIMatcher::exact(String("/api/override/") + name), HTTP_GET | HTTP_POST,
                   [this, pattern](AsyncWebServerRequest* request) {
                       logRequest(request);
                       applyOverrideRoute(request, pattern);
                   });
    }

    // Back to the mode: a one-URL route, plus DELETE on the collection.
    const auto clear = [this](AsyncWebServerRequest* request) {
        logRequest(request);
        controller_.clearOverride();
        handleGetState(request);
    };
    server_.on(AsyncURIMatcher::exact("/api/override/clear"), HTTP_GET | HTTP_POST, clear);
    server_.on(AsyncURIMatcher::exact("/api/override"), HTTP_DELETE, clear);

    // Wi-Fi network management. The /api/wifi/... routes MUST precede the generic
    // GET /api/wifi. Reset is POST-only so a browser prefetch can never trigger it.
    wifiAddHandler_.setMethod(HTTP_POST);
    server_.addHandler(&wifiAddHandler_);
    wifiForgetHandler_.setMethod(HTTP_POST);
    server_.addHandler(&wifiForgetHandler_);
    wifiKeepHandler_.setMethod(HTTP_POST);
    server_.addHandler(&wifiKeepHandler_);
    server_.on("/api/wifi/reset", HTTP_POST, [this](AsyncWebServerRequest* request) {
        logRequest(request);
        request->send(202, "application/json", "{\"status\":\"resetting\"}");
        wifiResetRequested_ = true;
    });
    server_.on("/api/wifi", HTTP_GET, [this](AsyncWebServerRequest* request) {
        logRequest(request);
        handleGetWifi(request);
    });

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
    overridePostHandler_.setMethod(HTTP_POST);
    server_.addHandler(&overridePostHandler_);
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

    // Any request that sets the mode (including the web UI's Save) drops an override.
    settings_.save(app);
    controller_.apply(app, /*clearOverride=*/!json["mode"].isNull());

    JsonDocument doc;
    SettingsCodec::writeSettings(doc, controller_.settings());
    String body;
    serializeJson(doc, body);
    request->send(200, "application/json", body);
}

void WebInterface::handleGetState(AsyncWebServerRequest* request) {
    JsonDocument doc;
    SettingsCodec::writeState(doc, controller_.status());

    String body;
    serializeJson(doc, body);
    request->send(200, "application/json", body);
}

void WebInterface::handlePostOverride(AsyncWebServerRequest* request, JsonVariant& json) {
    logRequest(request);
    if (!json.is<JsonObject>()) {
        sendError(request, 400, "expected JSON object");
        return;
    }

    Override ov;
    const SettingsCodec::Result parse =
        SettingsCodec::parseOverride(json.as<JsonObjectConst>(), controller_.settings(), ov);
    if (!parse.ok) {
        sendError(request, 400, parse.error);
        return;
    }
    controller_.setOverride(ov);
    handleGetState(request);
}

void WebInterface::logRequest(AsyncWebServerRequest* request) {
    Serial.printf("HTTP: %s %s from %s\n", request->methodToString(), request->url().c_str(),
                  request->client()->remoteIP().toString().c_str());
}

bool WebInterface::queryCommand(AsyncWebServerRequest* request, Mode pattern, JsonDocument& doc) {
    JsonObject obj = doc.to<JsonObject>();
    for (size_t i = 0; i < request->params(); i++) {
        const AsyncWebParameter* p = request->getParam(i);
        if (p->isPost() || p->isFile()) {
            continue;  // one-URL routes take no body
        }
        if (p->name() == "pattern") {
            sendError(request, 400, "the pattern comes from the path");
            return false;
        }
        SettingsCodec::putQueryParam(obj, p->name().c_str(), p->value().c_str());
    }
    obj["pattern"] = SettingsCodec::modeName(pattern);
    return true;
}

void WebInterface::applyModeRoute(AsyncWebServerRequest* request, Mode pattern) {
    JsonDocument doc;
    if (!queryCommand(request, pattern, doc)) {
        return;
    }
    AppSettings app;
    const SettingsCodec::Result parse =
        SettingsCodec::parseModeCommand(doc.as<JsonObjectConst>(), controller_.settings(), app);
    if (!parse.ok) {
        sendError(request, 400, parse.error);
        return;
    }
    settings_.save(app);
    controller_.apply(app, /*clearOverride=*/true);
    handleGetState(request);  // report the resulting state
}

void WebInterface::applyOverrideRoute(AsyncWebServerRequest* request, Mode pattern) {
    JsonDocument doc;
    if (!queryCommand(request, pattern, doc)) {
        return;
    }
    Override ov;
    const SettingsCodec::Result parse =
        SettingsCodec::parseOverride(doc.as<JsonObjectConst>(), controller_.settings(), ov);
    if (!parse.ok) {
        sendError(request, 400, parse.error);
        return;
    }
    controller_.setOverride(ov);
    handleGetState(request);  // report the resulting state
}

void WebInterface::handleGetWifi(AsyncWebServerRequest* request) {
    JsonDocument doc;
    WifiCodec::writeNetworks(doc, wifi_.snapshot());

    String body;
    serializeJson(doc, body);
    request->send(200, "application/json", body);
}

void WebInterface::handlePostWifiAdd(AsyncWebServerRequest* request, JsonVariant& json) {
    logRequest(request);
    const char* ssid     = nullptr;
    const char* password = nullptr;
    const WifiCodec::Result parse =
        json.is<JsonObject>() ? WifiCodec::parseAdd(json.as<JsonObjectConst>(), ssid, password)
                              : WifiCodec::Result{false, "expected JSON object"};
    if (!parse.ok) {
        sendError(request, 400, parse.error);
        return;
    }

    switch (wifi_.addNetwork(ssid, password)) {
        case KnownNetworks::AddResult::Added:
        case KnownNetworks::AddResult::Updated:
            handleGetWifi(request);
            break;
        case KnownNetworks::AddResult::Full:
            sendError(request, 409, "network list full; forget one first");
            break;
        default:
            sendError(request, 400, "invalid ssid or password");
            break;
    }
}

void WebInterface::handlePostWifiForget(AsyncWebServerRequest* request, JsonVariant& json) {
    logRequest(request);
    const char* ssid              = nullptr;
    const WifiCodec::Result parse = json.is<JsonObject>()
                                        ? WifiCodec::parseForget(json.as<JsonObjectConst>(), ssid)
                                        : WifiCodec::Result{false, "expected JSON object"};
    if (!parse.ok) {
        sendError(request, 400, parse.error);
        return;
    }

    if (!wifi_.forgetNetwork(ssid)) {
        sendError(request, 404, "network not found");
        return;
    }
    handleGetWifi(request);
}

void WebInterface::handlePostWifiKeep(AsyncWebServerRequest* request, JsonVariant& json) {
    logRequest(request);
    const char* replace           = nullptr;
    const WifiCodec::Result parse = json.is<JsonObject>()
                                        ? WifiCodec::parseKeep(json.as<JsonObjectConst>(), replace)
                                        : WifiCodec::Result{false, "expected JSON object"};
    if (!parse.ok) {
        sendError(request, 400, parse.error);
        return;
    }

    switch (wifi_.keepSessionNetwork(replace)) {
        case WifiProvisioner::KeepResult::Kept:
            handleGetWifi(request);
            break;
        case WifiProvisioner::KeepResult::NoSession:
            sendError(request, 404, "no session network");
            break;
        case WifiProvisioner::KeepResult::Full:
            sendError(request, 409, "network list full; specify replace");
            break;
        case WifiProvisioner::KeepResult::NotFound:
            sendError(request, 404, "replace target not found");
            break;
        default:
            sendError(request, 400, "invalid network");
            break;
    }
}

void WebInterface::sendError(AsyncWebServerRequest* request, int code, const char* error) {
    String body = String("{\"error\":\"") + error + "\"}";
    request->send(code, "application/json", body);
}
