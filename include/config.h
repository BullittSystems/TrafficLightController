/*
 * config.h
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

#include <stdint.h>

// Central hardware/firmware configuration.
// GPIO assignments resolve the "TBD" in docs/traffic-light-controller-hardware.md.
// These pins are boot-safe on the ESP32-C3 SuperMini: they avoid the BOOT/strap
// pins (GPIO2/8/9), the native USB pins (GPIO18/19), and UART0 (GPIO20/21).

// The three lamp channels. Values are stable (persisted and used as bit indices).
enum class Lamp : uint8_t { Red, Yellow, Green };

// One GPIO output per lamp channel. These are logic-level outputs; the driver
// electronics behind them (MOSFET, relay, LED driver, ...) are the integrator's
// choice and immaterial to the firmware.
static constexpr int RED_GPIO    = 3;
static constexpr int YELLOW_GPIO = 4;
static constexpr int GREEN_GPIO  = 5;

// Output polarity. true: a HIGH pin turns its lamp on (default). false: a LOW pin
// turns it on (active-low drivers, e.g. many relay boards). This is a wiring fact
// fixed per build, so it is compile-time config, not a runtime setting.
static constexpr bool LAMP_ACTIVE_HIGH = true;

// Wi-Fi provisioning access-point name (used in later tasks).
static constexpr char AP_SSID[] = "TrafficLight-Setup";

// Serial monitor baud (matches platformio.ini monitor_speed).
static constexpr unsigned long SERIAL_BAUD = 115200;

// Default per-phase durations (ms). Overridden at runtime by persisted Settings.
static constexpr uint32_t DEFAULT_GREEN_MS  = 3000;
static constexpr uint32_t DEFAULT_YELLOW_MS = 3000;
static constexpr uint32_t DEFAULT_RED_MS    = 3000;

// Traffic-light phase durations. Shared config contract between Settings and
// SequenceMode; defaults apply until overridden by persisted values.
struct PhaseDurations {
    uint32_t greenMs  = DEFAULT_GREEN_MS;
    uint32_t yellowMs = DEFAULT_YELLOW_MS;
    uint32_t redMs    = DEFAULT_RED_MS;
};

// Operating mode selected at runtime and persisted.
// NOTE: the numeric values are what get persisted, so reordering changes what a
// previously-saved mode reads back as. Keep Off last so it bounds the valid range.
enum class Mode : uint8_t { Api, Solid, Blink, Sequence, Random, Off };

// Default random-mode hold-time bounds (ms).
static constexpr uint32_t DEFAULT_RANDOM_MIN_MS = 1000;
static constexpr uint32_t DEFAULT_RANDOM_MAX_MS = 5000;

// Random-mode variant: Single lights one lamp per step; Multi lights a random
// non-empty subset (1-3 lamps) per step.
enum class RandomVariant : uint8_t { Single, Multi };

// Random-mode settings. Each step holds its target for a uniform-random duration
// in [minMs, maxMs].
struct RandomSettings {
    uint32_t minMs        = DEFAULT_RANDOM_MIN_MS;
    uint32_t maxMs        = DEFAULT_RANDOM_MAX_MS;
    RandomVariant variant = RandomVariant::Single;
};

// Solid mode: hold one color steady.
struct SolidSettings {
    Lamp color = Lamp::Green;
};

// Blink mode: flash one color, symmetric on/off at intervalMs.
static constexpr uint32_t DEFAULT_BLINK_MS = 500;
struct BlinkSettings {
    Lamp color          = Lamp::Red;
    uint32_t intervalMs = DEFAULT_BLINK_MS;
};

// Complete persisted application settings.
struct AppSettings {
    Mode mode = Mode::Sequence;
    PhaseDurations sequence;
    RandomSettings random;
    SolidSettings solid;
    BlinkSettings blink;
};

// Transient API-mode state (not persisted). Set via /api/state while in Api mode;
// resets to Off when entering Api mode and after reboot.
enum class ApiStateKind : uint8_t { Off, Solid, Blink };
struct ApiState {
    ApiStateKind kind   = ApiStateKind::Off;
    Lamp color          = Lamp::Red;
    uint32_t intervalMs = DEFAULT_BLINK_MS;
};
