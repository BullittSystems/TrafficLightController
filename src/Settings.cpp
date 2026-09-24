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
constexpr char NAMESPACE[]  = "traflight";
constexpr char KEY_GREEN[]  = "green_ms";
constexpr char KEY_YELLOW[] = "yellow_ms";
constexpr char KEY_RED[]    = "red_ms";
}  // namespace

PhaseDurations Settings::load() {
    PhaseDurations d;  // starts at compile-time defaults
    Preferences prefs;
    // Read-only open returns false on a namespace that does not exist yet; the
    // getULong defaults below then supply the compile-time values.
    prefs.begin(NAMESPACE, /*readOnly=*/true);
    d.greenMs  = prefs.getULong(KEY_GREEN, d.greenMs);
    d.yellowMs = prefs.getULong(KEY_YELLOW, d.yellowMs);
    d.redMs    = prefs.getULong(KEY_RED, d.redMs);
    prefs.end();
    return d;
}

void Settings::save(const PhaseDurations& durations) {
    Preferences prefs;
    prefs.begin(NAMESPACE, /*readOnly=*/false);
    prefs.putULong(KEY_GREEN, durations.greenMs);
    prefs.putULong(KEY_YELLOW, durations.yellowMs);
    prefs.putULong(KEY_RED, durations.redMs);
    prefs.end();
}
