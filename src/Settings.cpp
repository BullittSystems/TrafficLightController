/*
 * Settings.cpp
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
#include "Settings.h"

#include <Preferences.h>

namespace {
constexpr char NAMESPACE[]     = "traflight";
constexpr char KEY_MODE[]      = "mode";
constexpr char KEY_GREEN[]     = "green_ms";
constexpr char KEY_YELLOW[]    = "yellow_ms";
constexpr char KEY_RED[]       = "red_ms";
constexpr char KEY_RND_MIN[]   = "rnd_min_ms";
constexpr char KEY_RND_MAX[]   = "rnd_max_ms";
constexpr char KEY_RND_VAR[]   = "rnd_variant";
constexpr char KEY_SOLID_CLR[] = "solid_color";
constexpr char KEY_BLINK_CLR[] = "blink_color";
constexpr char KEY_BLINK_MS[]  = "blink_ms";
constexpr char KEY_CHASE_MS[]  = "chase_ms";

// Coerce a persisted byte back to a valid Lamp, defaulting on anything unexpected.
Lamp toLamp(uint8_t value, Lamp fallback) {
    switch (static_cast<Lamp>(value)) {
        case Lamp::Red:
        case Lamp::Yellow:
        case Lamp::Green:
            return static_cast<Lamp>(value);
    }
    return fallback;
}
}  // namespace

AppSettings Settings::load() {
    AppSettings s;  // starts at compile-time defaults
    Preferences prefs;
    // Read-only open returns false on a namespace that does not exist yet; the
    // get* defaults below then supply the compile-time values.
    prefs.begin(NAMESPACE, /*readOnly=*/true);
    s.mode              = static_cast<Mode>(prefs.getUChar(KEY_MODE, static_cast<uint8_t>(s.mode)));
    s.sequence.greenMs  = prefs.getULong(KEY_GREEN, s.sequence.greenMs);
    s.sequence.yellowMs = prefs.getULong(KEY_YELLOW, s.sequence.yellowMs);
    s.sequence.redMs    = prefs.getULong(KEY_RED, s.sequence.redMs);
    s.random.minMs      = prefs.getULong(KEY_RND_MIN, s.random.minMs);
    s.random.maxMs      = prefs.getULong(KEY_RND_MAX, s.random.maxMs);
    if (prefs.getUChar(KEY_RND_VAR, static_cast<uint8_t>(s.random.variant)) ==
        static_cast<uint8_t>(RandomVariant::Multi)) {
        s.random.variant = RandomVariant::Multi;
    } else {
        s.random.variant = RandomVariant::Single;
    }
    s.solid.color =
        toLamp(prefs.getUChar(KEY_SOLID_CLR, static_cast<uint8_t>(s.solid.color)), s.solid.color);
    s.blink.color =
        toLamp(prefs.getUChar(KEY_BLINK_CLR, static_cast<uint8_t>(s.blink.color)), s.blink.color);
    s.blink.intervalMs = prefs.getULong(KEY_BLINK_MS, s.blink.intervalMs);
    s.chase.intervalMs = prefs.getULong(KEY_CHASE_MS, s.chase.intervalMs);
    prefs.end();

    // Guard against a corrupt/out-of-range persisted mode value (Off is the last
    // enumerator, so it bounds the valid range).
    if (static_cast<uint8_t>(s.mode) > static_cast<uint8_t>(Mode::Off)) {
        s.mode = Mode::Sequence;
    }
    return s;
}

void Settings::save(const AppSettings& settings) {
    Preferences prefs;
    prefs.begin(NAMESPACE, /*readOnly=*/false);
    prefs.putUChar(KEY_MODE, static_cast<uint8_t>(settings.mode));
    prefs.putULong(KEY_GREEN, settings.sequence.greenMs);
    prefs.putULong(KEY_YELLOW, settings.sequence.yellowMs);
    prefs.putULong(KEY_RED, settings.sequence.redMs);
    prefs.putULong(KEY_RND_MIN, settings.random.minMs);
    prefs.putULong(KEY_RND_MAX, settings.random.maxMs);
    prefs.putUChar(KEY_RND_VAR, static_cast<uint8_t>(settings.random.variant));
    prefs.putUChar(KEY_SOLID_CLR, static_cast<uint8_t>(settings.solid.color));
    prefs.putUChar(KEY_BLINK_CLR, static_cast<uint8_t>(settings.blink.color));
    prefs.putULong(KEY_BLINK_MS, settings.blink.intervalMs);
    prefs.putULong(KEY_CHASE_MS, settings.chase.intervalMs);
    prefs.end();
}
