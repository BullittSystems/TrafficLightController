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

#include <string.h>

namespace SettingsCodec {

uint32_t clampMs(uint32_t value) {
    if (value < MIN_MS)
        return MIN_MS;
    if (value > MAX_MS)
        return MAX_MS;
    return value;
}

uint32_t clampTtlMs(uint32_t value) {
    if (value < MIN_TTL_MS)
        return MIN_TTL_MS;
    if (value > MAX_TTL_MS)
        return MAX_TTL_MS;
    return value;
}

const char* modeName(Mode mode) {
    switch (mode) {
        case Mode::Solid:
            return "solid";
        case Mode::Blink:
            return "blink";
        case Mode::Sequence:
            return "sequence";
        case Mode::Random:
            return "random";
        case Mode::Chase:
            return "chase";
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
    if (v == "solid") {
        out = Mode::Solid;
    } else if (v == "blink") {
        out = Mode::Blink;
    } else if (v == "sequence") {
        out = Mode::Sequence;
    } else if (v == "random") {
        out = Mode::Random;
    } else if (v == "chase") {
        out = Mode::Chase;
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
    JsonObject chase    = doc["chase"].to<JsonObject>();
    chase["intervalMs"] = s.chase.intervalMs;
}

void writeState(JsonDocument& doc, const LiveStatus& status) {
    const AppSettings& o = status.active.settings;
    doc["mode"]          = modeName(status.mode);
    doc["active"]        = modeName(status.overridden ? o.mode : status.mode);
    doc["lit"]           = litName(status.litMask);
    if (!status.overridden) {
        doc["override"] = nullptr;
        return;
    }

    // Only the pattern's own parameters, mirroring the POST /api/override body.
    JsonObject ov = doc["override"].to<JsonObject>();
    ov["pattern"] = modeName(o.mode);
    switch (o.mode) {
        case Mode::Solid:
            ov["color"] = colorName(o.solid.color);
            break;
        case Mode::Blink:
            ov["color"]      = colorName(o.blink.color);
            ov["intervalMs"] = o.blink.intervalMs;
            break;
        case Mode::Sequence:
            ov["greenMs"]  = o.sequence.greenMs;
            ov["yellowMs"] = o.sequence.yellowMs;
            ov["redMs"]    = o.sequence.redMs;
            break;
        case Mode::Random:
            ov["variant"] = randomVariantName(o.random.variant);
            ov["minMs"]   = o.random.minMs;
            ov["maxMs"]   = o.random.maxMs;
            break;
        case Mode::Chase:
            ov["intervalMs"] = o.chase.intervalMs;
            break;
        case Mode::Off:
            break;
    }
    if (status.active.ttlMs != 0) {
        ov["ttlMs"]       = status.active.ttlMs;
        ov["remainingMs"] = status.remainingMs;
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

    JsonVariantConst chase = obj["chase"];
    s.chase.intervalMs     = clampMs(chase["intervalMs"] | s.chase.intervalMs);

    return {true, nullptr};
}

namespace {
// True if `key` is a parameter of `pattern` (or ttlMs, for overrides).
bool acceptsKey(Mode pattern, const char* key, bool allowTtl) {
    if (strcmp(key, "pattern") == 0 || (allowTtl && strcmp(key, "ttlMs") == 0)) {
        return true;
    }
    switch (pattern) {
        case Mode::Solid:
            return strcmp(key, "color") == 0;
        case Mode::Blink:
            return strcmp(key, "color") == 0 || strcmp(key, "intervalMs") == 0;
        case Mode::Sequence:
            return strcmp(key, "greenMs") == 0 || strcmp(key, "yellowMs") == 0 ||
                   strcmp(key, "redMs") == 0;
        case Mode::Random:
            return strcmp(key, "variant") == 0 || strcmp(key, "minMs") == 0 ||
                   strcmp(key, "maxMs") == 0;
        case Mode::Chase:
            return strcmp(key, "intervalMs") == 0;
        case Mode::Off:
            return false;
    }
    return false;
}

// Optional duration: absent keeps `inout`; a non-negative integer is clamped.
bool readMs(JsonVariantConst v, uint32_t& inout) {
    if (v.isNull()) {
        return true;
    }
    if (!v.is<uint32_t>()) {
        return false;
    }
    inout = clampMs(v.as<uint32_t>());
    return true;
}

Result parsePattern(JsonObjectConst obj, AppSettings& s, bool allowTtl) {
    if (!parseMode(obj["pattern"], s.mode)) {
        return {false, "unknown pattern"};
    }
    for (JsonPairConst kv : obj) {
        if (!acceptsKey(s.mode, kv.key().c_str(), allowTtl)) {
            return {false, strcmp(kv.key().c_str(), "ttlMs") == 0
                               ? "ttlMs is only valid for overrides"
                               : "parameter not valid for this pattern"};
        }
    }

    constexpr Result badMs = {false, "durations must be non-negative integers"};
    JsonVariantConst color = obj["color"];
    switch (s.mode) {
        case Mode::Solid:
            if (!color.isNull() && !parseColor(color, s.solid.color)) {
                return {false, "invalid color"};
            }
            break;
        case Mode::Blink:
            if (!color.isNull() && !parseColor(color, s.blink.color)) {
                return {false, "invalid color"};
            }
            if (!readMs(obj["intervalMs"], s.blink.intervalMs)) {
                return badMs;
            }
            break;
        case Mode::Sequence:
            if (!readMs(obj["greenMs"], s.sequence.greenMs) ||
                !readMs(obj["yellowMs"], s.sequence.yellowMs) ||
                !readMs(obj["redMs"], s.sequence.redMs)) {
                return badMs;
            }
            break;
        case Mode::Random:
            if (!obj["variant"].isNull() && !parseVariant(obj["variant"], s.random.variant)) {
                return {false, "invalid random variant"};
            }
            if (!readMs(obj["minMs"], s.random.minMs) || !readMs(obj["maxMs"], s.random.maxMs)) {
                return badMs;
            }
            if (s.random.maxMs < s.random.minMs) {
                s.random.maxMs = s.random.minMs;
            }
            break;
        case Mode::Chase:
            if (!readMs(obj["intervalMs"], s.chase.intervalMs)) {
                return badMs;
            }
            break;
        case Mode::Off:
            break;
    }
    return {true, nullptr};
}
}  // namespace

Result parseOverride(JsonObjectConst obj, const AppSettings& saved, Override& out) {
    out.settings        = saved;
    out.ttlMs           = 0;
    const Result result = parsePattern(obj, out.settings, /*allowTtl=*/true);
    if (!result.ok) {
        return result;
    }
    JsonVariantConst ttl = obj["ttlMs"];
    if (!ttl.isNull()) {
        if (!ttl.is<uint32_t>() || ttl.as<uint32_t>() == 0) {
            return {false, "ttlMs must be a positive integer"};
        }
        out.ttlMs = clampTtlMs(ttl.as<uint32_t>());
    }
    return {true, nullptr};
}

Result parseModeCommand(JsonObjectConst obj, const AppSettings& saved, AppSettings& out) {
    out = saved;
    return parsePattern(obj, out, /*allowTtl=*/false);
}

void putQueryParam(JsonObject obj, const char* name, const char* value) {
    uint64_t number = 0;
    bool digits     = *value != '\0';
    for (const char* p = value; digits && *p != '\0'; p++) {
        digits = *p >= '0' && *p <= '9';
        number = number * 10 + static_cast<uint64_t>(*p - '0');
        if (number > UINT32_MAX) {
            digits = false;
        }
    }
    if (digits) {
        obj[name] = static_cast<uint32_t>(number);
    } else {
        obj[name] = value;
    }
}

}  // namespace SettingsCodec
