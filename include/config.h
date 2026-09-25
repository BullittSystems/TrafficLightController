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

// The three lamp channels. Values are stable (persisted and used as bit indices).
enum class Lamp : uint8_t { Red, Yellow, Green };

// One GPIO output per lamp channel. These are logic-level outputs; the driver
// electronics behind them (MOSFET, relay, LED driver, ...) are the integrator's
// choice and immaterial to the firmware. Pins are board-specific (selected by the
// TLC_BOARD_* build flag from platformio.ini), chosen to avoid each chip's
// strapping / USB / UART / flash pins.
#if defined(TLC_BOARD_ESP32)
// AITRIP/LOLIN D1 Mini ESP32 (ESP-WROOM-32). GPIO22/21/17 are three consecutive
// pins in the inner-left header column (GND is two rows below them); all are safe
// outputs. BOOT button is on GPIO0. (Avoid 0/2/12/15 strap, 1/3 UART0, 6-11 flash,
// 34-39 input-only.)
static constexpr int RED_GPIO          = 22;
static constexpr int YELLOW_GPIO       = 21;
static constexpr int GREEN_GPIO        = 17;
static constexpr int RESET_BUTTON_GPIO = 0;
#else  // TLC_BOARD_C3 (default): Teyleten ESP32-C3 SuperMini
// GPIO3/4/5 are safe; BOOT button on GPIO9. (Avoid 2/8/9 strap, 18/19 USB, 20/21 UART0.)
static constexpr int RED_GPIO          = 3;
static constexpr int YELLOW_GPIO       = 4;
static constexpr int GREEN_GPIO        = 5;
static constexpr int RESET_BUTTON_GPIO = 9;
#endif

// Output polarity. true: a HIGH pin turns its lamp on (default). false: a LOW pin
// turns it on (active-low drivers, e.g. many relay boards). This is a wiring fact
// fixed per build, so it is compile-time config, not a runtime setting.
static constexpr bool LAMP_ACTIVE_HIGH = true;

// Wi-Fi provisioning access-point name.
static constexpr char AP_SSID[] = "TrafficLight-Setup";

// How long to attempt a background connect with saved credentials before giving up
// and opening the provisioning portal. The attempt is non-blocking either way.
static constexpr uint32_t WIFI_STA_CONNECT_TIMEOUT_MS = 15000;

// Upper bound on the async scan for known networks before falling back to the portal.
static constexpr uint32_t WIFI_SCAN_TIMEOUT_MS = 10000;

// Bounds the (blocking) connect WiFiManager performs when the portal form is saved.
static constexpr unsigned long WIFI_PORTAL_CONNECT_TIMEOUT_S = 10;

// Two-tier BOOT button (RESET_BUTTON_GPIO, set per board above), held while the
// firmware is running. Past WIFI_PORTAL_HOLD_MS the lamps show solid yellow; releasing
// then restarts into the provisioning portal with the saved networks kept. Holding
// on to WIFI_RESET_HOLD_MS forgets every network instead. Must be held after boot,
// not through reset (the BOOT pin low at reset enters the ROM bootloader).
static constexpr uint32_t WIFI_PORTAL_HOLD_MS = 5000;
static constexpr uint32_t WIFI_RESET_HOLD_MS  = 15000;

// Retry interval for binding the web server while port 80 is still held after a
// portal session.
static constexpr uint32_t WEB_BIND_RETRY_MS = 5000;

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
// previously-saved mode reads back as. Keep Off last so it bounds the valid range;
// insert new modes before it (and update the range guard in Settings::load).
enum class Mode : uint8_t { Api, Solid, Blink, Sequence, Random, Chase, Off };

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

// Chase mode: walk R -> R+Y -> Y -> Y+G -> G -> G+R and repeat, each step held for
// intervalMs.
static constexpr uint32_t DEFAULT_CHASE_MS = 250;
struct ChaseSettings {
    uint32_t intervalMs = DEFAULT_CHASE_MS;
};

// Complete persisted application settings.
struct AppSettings {
    Mode mode = Mode::Sequence;
    PhaseDurations sequence;
    RandomSettings random;
    SolidSettings solid;
    BlinkSettings blink;
    ChaseSettings chase;
};

// Transient API-mode state (not persisted). Set via /api/state while in Api mode;
// resets to Off when entering Api mode and after reboot.
enum class ApiStateKind : uint8_t { Off, Solid, Blink };
struct ApiState {
    ApiStateKind kind   = ApiStateKind::Off;
    Lamp color          = Lamp::Red;
    uint32_t intervalMs = DEFAULT_BLINK_MS;
};
