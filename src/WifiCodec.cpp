/*
 * WifiCodec.cpp
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
#include "WifiCodec.h"

namespace WifiCodec {

namespace {
void writeOptional(JsonDocument& doc, const char* key, const char* value) {
    if (value[0] != '\0') {
        doc[key] = value;
    } else {
        doc[key] = nullptr;
    }
}
}  // namespace

void writeNetworks(JsonDocument& doc, const WifiSnapshot& snap) {
    JsonArray networks = doc["networks"].to<JsonArray>();
    for (uint8_t i = 0; i < snap.count; i++) {
        JsonObject net = networks.add<JsonObject>();
        net["ssid"]    = snap.ssids[i];
    }
    writeOptional(doc, "session", snap.session);
    writeOptional(doc, "connected", snap.connected);
    doc["max"] = MAX_KNOWN_NETWORKS;
}

Result parseAdd(JsonObjectConst obj, const char*& ssid, const char*& password) {
    if (!obj["ssid"].is<const char*>()) {
        return {false, "ssid must be a string"};
    }
    JsonVariantConst pass = obj["password"];
    if (!pass.isNull() && !pass.is<const char*>()) {
        return {false, "password must be a string"};
    }
    ssid     = obj["ssid"].as<const char*>();
    password = pass.isNull() ? "" : pass.as<const char*>();
    return {true, nullptr};
}

Result parseForget(JsonObjectConst obj, const char*& ssid) {
    if (!obj["ssid"].is<const char*>()) {
        return {false, "ssid must be a string"};
    }
    ssid = obj["ssid"].as<const char*>();
    return {true, nullptr};
}

Result parseKeep(JsonObjectConst obj, const char*& replace) {
    JsonVariantConst r = obj["replace"];
    if (!r.isNull() && !r.is<const char*>()) {
        return {false, "replace must be a string"};
    }
    replace = r.isNull() ? nullptr : r.as<const char*>();
    return {true, nullptr};
}

}  // namespace WifiCodec
