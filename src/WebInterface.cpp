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

namespace {
// Accepted range for any phase duration (ms). Enforced server-side regardless of
// client input: rules out 0/near-zero (which would spin the sequencer) and values
// so large they appear hung.
constexpr uint32_t MIN_MS = 100;
constexpr uint32_t MAX_MS = 600000;

uint32_t clampMs(uint32_t v) {
    if (v < MIN_MS)
        return MIN_MS;
    if (v > MAX_MS)
        return MAX_MS;
    return v;
}

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
          padding: .6rem 0; border-bottom: 1px solid #8884; }
  input { width: 8rem; padding: .3rem; font-variant-numeric: tabular-nums; }
  button { margin-top: 1rem; padding: .5rem 1rem; }
  .muted { color: #888; font-size: .85rem; }
</style>
</head>
<body>
  <h1>Traffic Light</h1>
  <form id="f">
    <label>Green <input type="number" name="greenMs" min="100" max="600000" step="50" required></label>
    <label>Yellow <input type="number" name="yellowMs" min="100" max="600000" step="50" required></label>
    <label>Red <input type="number" name="redMs" min="100" max="600000" step="50" required></label>
    <button type="submit">Save</button>
  </form>
  <p id="status" class="muted">Phase durations in milliseconds.</p>
<script>
  const f = document.getElementById('f');
  const statusEl = document.getElementById('status');
  function fill(s) {
    f.greenMs.value = s.greenMs;
    f.yellowMs.value = s.yellowMs;
    f.redMs.value = s.redMs;
  }
  async function load() {
    fill(await (await fetch('/api/settings')).json());
  }
  f.addEventListener('submit', async (e) => {
    e.preventDefault();
    const body = { greenMs: +f.greenMs.value, yellowMs: +f.yellowMs.value, redMs: +f.redMs.value };
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

WebInterface::WebInterface(Settings& settings, Sequencer& sequencer)
    : server_(80),
      postHandler_("/api/settings",
                   [this](AsyncWebServerRequest* request, JsonVariant& json) {
                       handlePostSettings(request, json);
                   }),
      settings_(settings),
      sequencer_(sequencer) {}

void WebInterface::begin() {
    server_.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        // ESP32 flash is memory-mapped, so the PROGMEM literal is read directly.
        request->send(200, "text/html", INDEX_HTML);
    });
    server_.on("/api/settings", HTTP_GET,
               [this](AsyncWebServerRequest* request) { handleGetSettings(request); });

    postHandler_.setMethod(HTTP_POST);
    server_.addHandler(&postHandler_);

    server_.begin();
}

void WebInterface::handleGetSettings(AsyncWebServerRequest* request) {
    const PhaseDurations& d = sequencer_.durations();

    JsonDocument doc;
    doc["greenMs"]  = d.greenMs;
    doc["yellowMs"] = d.yellowMs;
    doc["redMs"]    = d.redMs;

    String body;
    serializeJson(doc, body);
    request->send(200, "application/json", body);
}

void WebInterface::handlePostSettings(AsyncWebServerRequest* request, JsonVariant& json) {
    if (!json.is<JsonObject>()) {
        request->send(400, "application/json", "{\"error\":\"expected JSON object\"}");
        return;
    }
    JsonObject obj = json.as<JsonObject>();

    // Start from current values so absent/invalid fields are left unchanged; the
    // "| default" form yields the current value when a key is missing or mistyped.
    PhaseDurations d = sequencer_.durations();
    d.greenMs        = clampMs(obj["greenMs"] | d.greenMs);
    d.yellowMs       = clampMs(obj["yellowMs"] | d.yellowMs);
    d.redMs          = clampMs(obj["redMs"] | d.redMs);

    settings_.save(d);
    sequencer_.setDurations(d);

    JsonDocument doc;
    doc["greenMs"]  = d.greenMs;
    doc["yellowMs"] = d.yellowMs;
    doc["redMs"]    = d.redMs;

    String body;
    serializeJson(doc, body);
    request->send(200, "application/json", body);
}
