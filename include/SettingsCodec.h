/*
 * SettingsCodec.h
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

#include <ArduinoJson.h>
#include <stdint.h>

#include "config.h"

// Pure (de)serialization and validation of settings/state as JSON. Kept free of
// any web-server dependency so it can be unit-tested on the host.
namespace SettingsCodec {

// Accepted range for any duration (ms), enforced regardless of client input.
constexpr uint32_t MIN_MS = 100;
constexpr uint32_t MAX_MS = 600000;

// Accepted range for an override's ttlMs (1 s to 24 h).
constexpr uint32_t MIN_TTL_MS = 1000;
constexpr uint32_t MAX_TTL_MS = 86400000;

// Outcome of a parse: ok, or a short error message for a 400 response.
struct Result {
    bool ok;
    const char* error;
};

uint32_t clampMs(uint32_t value);
uint32_t clampTtlMs(uint32_t value);

const char* modeName(Mode mode);
const char* colorName(Lamp color);
const char* randomVariantName(RandomVariant variant);
const char* litName(uint8_t mask);

bool parseMode(JsonVariantConst v, Mode& out);
bool parseColor(JsonVariantConst v, Lamp& out);
bool parseVariant(JsonVariantConst v, RandomVariant& out);

// Serialize full settings to the nested API shape.
void writeSettings(JsonDocument& doc, const AppSettings& s);

// Serialize the GET /api/state response: persisted mode, the pattern actually
// running, lit lamps, and the override (or null).
void writeState(JsonDocument& doc, const LiveStatus& status);

// Merge a settings patch (nested JSON) onto `s`. Absent fields keep their value;
// a present-but-invalid field fails without further change being meaningful (the
// caller discards `s` on failure).
Result applySettingsPatch(JsonObjectConst obj, AppSettings& s);

// Pattern commands: {"pattern", ...flat params}. The result starts from `saved`, so
// omitted parameters keep their saved values. Each pattern accepts only its own
// parameters; anything else is rejected:
//   solid: color            blink: color, intervalMs    sequence: greenMs, yellowMs, redMs
//   random: variant, minMs, maxMs                        chase: intervalMs    off: none
// Durations must be non-negative integers (then clamped); ttlMs a positive one.

// An override (POST /api/override body, or a one-URL route's query): also accepts
// ttlMs, clamped to MIN_TTL_MS..MAX_TTL_MS.
Result parseOverride(JsonObjectConst obj, const AppSettings& saved, Override& out);

// A persisted mode switch (a one-URL /api/mode route's query). ttlMs is rejected.
Result parseModeCommand(JsonObjectConst obj, const AppSettings& saved, AppSettings& out);

// Add one query-string parameter to `obj` for the parsers above: a value of only
// digits that fits in uint32 becomes a number, anything else stays a string (so a
// bad duration is rejected by the parser, not silently dropped).
void putQueryParam(JsonObject obj, const char* name, const char* value);

}  // namespace SettingsCodec
