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
// Accepted range for any duration (ms). Enforced server-side regardless of client
// input: rules out 0/near-zero (which would spin a mode) and values so large they
// appear hung.
constexpr uint32_t MIN_MS = 100;
constexpr uint32_t MAX_MS = 600000;

uint32_t clampMs(uint32_t v) {
    if (v < MIN_MS)
        return MIN_MS;
    if (v > MAX_MS)
        return MAX_MS;
    return v;
}

const char* colorName(Lamp color) {
    switch (color) {
        case Lamp::Red:
            return "red";
        case Lamp::Yellow:
            return "yellow";
        case Lamp::Green:
            return "green";
    }
    return "red";
}

// Parse a color string into a Lamp. Returns false for any unrecognized value.
bool parseColor(JsonVariantConst v, Lamp& out) {
    if (v == "red") {
        out = Lamp::Red;
    } else if (v == "yellow") {
        out = Lamp::Yellow;
    } else if (v == "green") {
        out = Lamp::Green;
    } else {
        return false;
    }
    return true;
}

const char* modeName(Mode mode) {
    switch (mode) {
        case Mode::Sequence:
            return "sequence";
        case Mode::Random:
            return "random";
        case Mode::Solid:
            return "solid";
        case Mode::Blink:
            return "blink";
        case Mode::Off:
            return "off";
    }
    return "sequence";
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
        <option value="sequence">Sequence</option>
        <option value="random">Random</option>
        <option value="solid">Solid</option>
        <option value="blink">Blink</option>
        <option value="off">Off</option>
      </select>
    </label>
    <fieldset id="seq">
      <legend>Sequence durations (ms)</legend>
      <label>Green <input type="number" name="greenMs" min="100" max="600000" step="50" required></label>
      <label>Yellow <input type="number" name="yellowMs" min="100" max="600000" step="50" required></label>
      <label>Red <input type="number" name="redMs" min="100" max="600000" step="50" required></label>
    </fieldset>
    <fieldset id="rnd">
      <legend>Random hold time (ms)</legend>
      <label>Min <input type="number" name="minMs" min="100" max="600000" step="50" required></label>
      <label>Max <input type="number" name="maxMs" min="100" max="600000" step="50" required></label>
    </fieldset>
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
    <button type="submit">Save</button>
  </form>
  <p id="status" class="muted">&nbsp;</p>
<script>
  const f = document.getElementById('f');
  const statusEl = document.getElementById('status');
  const fs = {
    sequence: document.getElementById('seq'),
    random: document.getElementById('rnd'),
    solid: document.getElementById('solid'),
    blink: document.getElementById('blink')
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
      random: { minMs: +f.minMs.value, maxMs: +f.maxMs.value },
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

// Serialize the full settings object to the nested API shape.
void writeSettings(JsonDocument& doc, const AppSettings& s) {
    doc["mode"]         = modeName(s.mode);
    JsonObject seq      = doc["sequence"].to<JsonObject>();
    seq["greenMs"]      = s.sequence.greenMs;
    seq["yellowMs"]     = s.sequence.yellowMs;
    seq["redMs"]        = s.sequence.redMs;
    JsonObject rnd      = doc["random"].to<JsonObject>();
    rnd["minMs"]        = s.random.minMs;
    rnd["maxMs"]        = s.random.maxMs;
    JsonObject solid    = doc["solid"].to<JsonObject>();
    solid["color"]      = colorName(s.solid.color);
    JsonObject blink    = doc["blink"].to<JsonObject>();
    blink["color"]      = colorName(s.blink.color);
    blink["intervalMs"] = s.blink.intervalMs;
}
}  // namespace

WebInterface::WebInterface(Settings& settings, ModeController& controller)
    : server_(80),
      postHandler_("/api/settings",
                   [this](AsyncWebServerRequest* request, JsonVariant& json) {
                       handlePostSettings(request, json);
                   }),
      settings_(settings),
      controller_(controller) {}

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
    JsonDocument doc;
    writeSettings(doc, controller_.settings());

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

    // Start from current settings so absent/invalid fields are left unchanged; the
    // "| default" form yields the current value when a key is missing or mistyped.
    AppSettings app = controller_.settings();

    // Mode: update only if present; reject a present-but-unknown value.
    JsonVariant mode = obj["mode"];
    if (!mode.isNull()) {
        if (mode == "sequence") {
            app.mode = Mode::Sequence;
        } else if (mode == "random") {
            app.mode = Mode::Random;
        } else if (mode == "solid") {
            app.mode = Mode::Solid;
        } else if (mode == "blink") {
            app.mode = Mode::Blink;
        } else if (mode == "off") {
            app.mode = Mode::Off;
        } else {
            request->send(400, "application/json", "{\"error\":\"unknown mode\"}");
            return;
        }
    }

    JsonObject seq        = obj["sequence"];
    app.sequence.greenMs  = clampMs(seq["greenMs"] | app.sequence.greenMs);
    app.sequence.yellowMs = clampMs(seq["yellowMs"] | app.sequence.yellowMs);
    app.sequence.redMs    = clampMs(seq["redMs"] | app.sequence.redMs);

    JsonObject rnd   = obj["random"];
    app.random.minMs = clampMs(rnd["minMs"] | app.random.minMs);
    app.random.maxMs = clampMs(rnd["maxMs"] | app.random.maxMs);
    if (app.random.maxMs < app.random.minMs) {
        app.random.maxMs = app.random.minMs;
    }

    // Colors: update only if present; reject a present-but-invalid value.
    JsonObject solid = obj["solid"];
    if (!solid["color"].isNull() && !parseColor(solid["color"], app.solid.color)) {
        request->send(400, "application/json", "{\"error\":\"invalid solid color\"}");
        return;
    }
    JsonObject blink = obj["blink"];
    if (!blink["color"].isNull() && !parseColor(blink["color"], app.blink.color)) {
        request->send(400, "application/json", "{\"error\":\"invalid blink color\"}");
        return;
    }
    app.blink.intervalMs = clampMs(blink["intervalMs"] | app.blink.intervalMs);

    settings_.save(app);
    controller_.apply(app);

    JsonDocument doc;
    writeSettings(doc, controller_.settings());

    String body;
    serializeJson(doc, body);
    request->send(200, "application/json", body);
}
