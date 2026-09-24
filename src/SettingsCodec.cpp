/*
 * SettingsCodec.cpp
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
#include "SettingsCodec.h"

namespace SettingsCodec {

uint32_t clampMs(uint32_t value) {
    if (value < MIN_MS)
        return MIN_MS;
    if (value > MAX_MS)
        return MAX_MS;
    return value;
}

const char* modeName(Mode mode) {
    switch (mode) {
        case Mode::Api:
            return "api";
        case Mode::Solid:
            return "solid";
        case Mode::Blink:
            return "blink";
        case Mode::Sequence:
            return "sequence";
        case Mode::Random:
            return "random";
        case Mode::Off:
            return "off";
    }
    return "sequence";
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

const char* randomVariantName(RandomVariant variant) {
    return variant == RandomVariant::Multi ? "multi" : "single";
}

const char* apiKindName(ApiStateKind kind) {
    switch (kind) {
        case ApiStateKind::Off:
            return "off";
        case ApiStateKind::Solid:
            return "solid";
        case ApiStateKind::Blink:
            return "blink";
    }
    return "off";
}

const char* litName(uint8_t mask) {
    switch (mask) {
        case 0:
            return "off";
        case (1u << static_cast<int>(Lamp::Red)):
            return "red";
        case (1u << static_cast<int>(Lamp::Yellow)):
            return "yellow";
        case (1u << static_cast<int>(Lamp::Green)):
            return "green";
        default:
            return "multi";
    }
}

bool parseMode(JsonVariantConst v, Mode& out) {
    if (v == "api") {
        out = Mode::Api;
    } else if (v == "solid") {
        out = Mode::Solid;
    } else if (v == "blink") {
        out = Mode::Blink;
    } else if (v == "sequence") {
        out = Mode::Sequence;
    } else if (v == "random") {
        out = Mode::Random;
    } else if (v == "off") {
        out = Mode::Off;
    } else {
        return false;
    }
    return true;
}

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

bool parseVariant(JsonVariantConst v, RandomVariant& out) {
    if (v == "single") {
        out = RandomVariant::Single;
    } else if (v == "multi") {
        out = RandomVariant::Multi;
    } else {
        return false;
    }
    return true;
}

bool parseApiKind(JsonVariantConst v, ApiStateKind& out) {
    if (v == "off") {
        out = ApiStateKind::Off;
    } else if (v == "solid") {
        out = ApiStateKind::Solid;
    } else if (v == "blink") {
        out = ApiStateKind::Blink;
    } else {
        return false;
    }
    return true;
}

void writeSettings(JsonDocument& doc, const AppSettings& s) {
    doc["mode"]         = modeName(s.mode);
    JsonObject seq      = doc["sequence"].to<JsonObject>();
    seq["greenMs"]      = s.sequence.greenMs;
    seq["yellowMs"]     = s.sequence.yellowMs;
    seq["redMs"]        = s.sequence.redMs;
    JsonObject rnd      = doc["random"].to<JsonObject>();
    rnd["minMs"]        = s.random.minMs;
    rnd["maxMs"]        = s.random.maxMs;
    rnd["variant"]      = randomVariantName(s.random.variant);
    JsonObject solid    = doc["solid"].to<JsonObject>();
    solid["color"]      = colorName(s.solid.color);
    JsonObject blink    = doc["blink"].to<JsonObject>();
    blink["color"]      = colorName(s.blink.color);
    blink["intervalMs"] = s.blink.intervalMs;
}

void writeState(JsonDocument& doc, Mode mode, uint8_t litMask, bool inApiMode,
                const ApiState& apiState) {
    doc["mode"] = modeName(mode);
    doc["lit"]  = litName(litMask);
    if (inApiMode) {
        JsonObject state    = doc["state"].to<JsonObject>();
        state["state"]      = apiKindName(apiState.kind);
        state["color"]      = colorName(apiState.color);
        state["intervalMs"] = apiState.intervalMs;
    } else {
        doc["state"] = nullptr;
    }
}

Result applySettingsPatch(JsonObjectConst obj, AppSettings& s) {
    JsonVariantConst mode = obj["mode"];
    if (!mode.isNull() && !parseMode(mode, s.mode)) {
        return {false, "unknown mode"};
    }

    JsonVariantConst seq = obj["sequence"];
    s.sequence.greenMs   = clampMs(seq["greenMs"] | s.sequence.greenMs);
    s.sequence.yellowMs  = clampMs(seq["yellowMs"] | s.sequence.yellowMs);
    s.sequence.redMs     = clampMs(seq["redMs"] | s.sequence.redMs);

    JsonVariantConst rnd = obj["random"];
    s.random.minMs       = clampMs(rnd["minMs"] | s.random.minMs);
    s.random.maxMs       = clampMs(rnd["maxMs"] | s.random.maxMs);
    if (s.random.maxMs < s.random.minMs) {
        s.random.maxMs = s.random.minMs;
    }
    if (!rnd["variant"].isNull() && !parseVariant(rnd["variant"], s.random.variant)) {
        return {false, "invalid random variant"};
    }

    JsonVariantConst solid = obj["solid"];
    if (!solid["color"].isNull() && !parseColor(solid["color"], s.solid.color)) {
        return {false, "invalid solid color"};
    }

    JsonVariantConst blink = obj["blink"];
    if (!blink["color"].isNull() && !parseColor(blink["color"], s.blink.color)) {
        return {false, "invalid blink color"};
    }
    s.blink.intervalMs = clampMs(blink["intervalMs"] | s.blink.intervalMs);

    return {true, nullptr};
}

Result parseApiCommand(JsonObjectConst obj, ApiState& state) {
    if (!parseApiKind(obj["state"], state.kind)) {
        return {false, "unknown state"};
    }
    if (state.kind != ApiStateKind::Off && !parseColor(obj["color"], state.color)) {
        return {false, "invalid color"};
    }
    state.intervalMs = clampMs(obj["intervalMs"] | state.intervalMs);
    return {true, nullptr};
}

}  // namespace SettingsCodec
