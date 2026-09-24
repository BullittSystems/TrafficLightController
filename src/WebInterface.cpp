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
  .row { display: flex; justify-content: space-between; padding: .6rem 0;
         border-bottom: 1px solid #8884; }
  .label { text-transform: capitalize; }
  .val { font-variant-numeric: tabular-nums; }
  .muted { color: #888; font-size: .85rem; }
</style>
</head>
<body>
  <h1>Traffic Light</h1>
  <div id="settings">Loading&hellip;</div>
  <p class="muted">Phase durations (read-only)</p>
<script>
  async function load() {
    const s = await (await fetch('/api/settings')).json();
    const rows = [['Green', s.greenMs], ['Yellow', s.yellowMs], ['Red', s.redMs]];
    document.getElementById('settings').innerHTML = rows.map(
      ([k, v]) => `<div class="row"><span class="label">${k}</span><span class="val">${v} ms</span></div>`
    ).join('');
  }
  load();
</script>
</body>
</html>)HTML";
}  // namespace

WebInterface::WebInterface(Sequencer& sequencer) : server_(80), sequencer_(sequencer) {}

void WebInterface::begin() {
    server_.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send_P(200, "text/html", INDEX_HTML);
    });
    server_.on("/api/settings", HTTP_GET,
               [this](AsyncWebServerRequest* request) { handleGetSettings(request); });
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
