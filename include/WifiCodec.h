/*
 * WifiCodec.h
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

#include "KnownNetworks.h"

// Pure JSON (de)serialization for the /api/wifi routes. Shape checks only; SSID and
// password content is validated by KnownNetworks. Parsed strings borrow from the
// JSON document, so they are valid only while it lives. No web-server dependency,
// so it can be unit-tested on the host.
namespace WifiCodec {

// Outcome of a parse: ok, or a short error message for a 400 response.
struct Result {
    bool ok;
    const char* error;
};

// Serialize GET /api/wifi: {"networks":[{"ssid"}], "session", "connected", "max"}.
// Empty snapshot strings become null.
void writeNetworks(JsonDocument& doc, const WifiSnapshot& snap);

// POST /api/wifi/add: {"ssid": string, "password"?: string}. Password defaults to
// "" (open network).
Result parseAdd(JsonObjectConst obj, const char*& ssid, const char*& password);

// POST /api/wifi/forget: {"ssid": string}.
Result parseForget(JsonObjectConst obj, const char*& ssid);

// POST /api/wifi/keep: {"replace"?: string}. replace is null when absent.
Result parseKeep(JsonObjectConst obj, const char*& replace);

}  // namespace WifiCodec
