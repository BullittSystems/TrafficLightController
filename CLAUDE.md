# CLAUDE.md

Project-specific context for the Traffic Light Controller firmware. General
working-style preferences live in the user's global config; this file holds facts
about *this* repo.

## What this is

Firmware for a standard three-aspect **red/yellow/green** signal on a **Teyleten
Robot ESP32-C3 SuperMini**. The contract is hardware-agnostic: **three GPIO outputs,
one per lamp, driven on/off** (active-high by default; `LAMP_ACTIVE_HIGH` in
`config.h` flips polarity for active-low drivers). Voltage/current/driver topology
are the integrator's concern, not the firmware's. The device joins Wi-Fi, serves a
web UI, and exposes a REST API. The **reference build** (12V bulbs + N-channel
MOSFETs) is documented in
[docs/traffic-light-controller-hardware.md](docs/traffic-light-controller-hardware.md).

## Build / flash / test

PlatformIO. On this dev machine `pio` is at `~/.platformio/penv/bin/pio` (not on PATH).

- Build firmware: `pio run` (default env `esp32-c3-supermini`)
- Flash: `pio run -t upload`
- Serial monitor: `pio device monitor` (115200)
- Unit tests (host): `pio test -e native`

### Toolchain gotcha (Apple Silicon)

The `espressif32` platform ships precompiled IDF archives built with an
**x86_64-only gcc 8.4** toolchain. On Apple Silicon it runs under **Rosetta 2**
(`softwareupdate --install-rosetta`). Do **not** pin a newer native toolchain:
gcc 15 fails to assemble the core (`zicsr`) and ABI-mismatches the precompiled
archives. CI runs on x86_64 Linux, so it builds natively there.

## Architecture

Decoupled layers; lamp behavior never depends on the network.

- `LampController` — owns the 3 GPIOs; boot-safe `begin()` drives all to the OFF
  level first. Polarity configurable (`activeHigh` ctor arg, default `LAMP_ACTIVE_HIGH`).
- `StepMode` — generic engine that walks a `Step{lampMask, durationMs}` table
  (`durationMs==0` = hold forever). Backs Sequence/Solid/Blink/Off + API states.
- `RandomMode` — algorithmic; single (one lamp, never repeats) or multi (random
  non-empty subset, always changes); uses `esp_random()`.
- `ModeController` — owns the active mode + transient API state; builds step tables;
  `apply()` (persisted config) / `applyApiState()` (transient). **Guarded by a
  FreeRTOS mutex** because web handlers and `loop()` run on different tasks.
- `Settings` — persists `AppSettings` to NVS (Preferences).
- `SettingsCodec` — pure JSON parse/validate/serialize (no web-server dep, unit-tested).
- `WebInterface` — ESPAsyncWebServer on :80; thin handlers delegate to the codec.
- `WifiProvisioner` — WiFiManager, non-blocking; AP portal `TrafficLight-Setup`;
  `resetCredentials()` clears saved creds. `main` polls the BOOT button (GPIO9,
  `RESET_BUTTON_GPIO`); holding it `WIFI_RESET_HOLD_MS` clears creds + restarts into
  the portal (poll in `loop()`, not at boot — GPIO9 low at reset enters the bootloader).

Boot flow (`main.cpp`): lamps LOW -> load settings -> `controller.apply()` (starts
mode immediately) -> Wi-Fi in background -> web server once connected.

### Concurrency

ESPAsyncWebServer callbacks run on the **AsyncTCP task**, not `loop()`. Any shared
mode/lamp state a handler touches must go through `ModeController`, which serializes
with a mutex. Do not mutate `StepMode`/`RandomMode` directly from a handler.

### Routing gotcha

ESPAsyncWebServer matches URIs by **prefix in registration order**. Register
specific routes (`/api/state/...`, `/api/mode/...`) **before** generic ones
(`/api/state`, `/api/settings`) or the generic one shadows them.

## Modes and API

Modes (persisted): `api`, `solid`, `blink`, `sequence`, `random`, `off`
(enum order in `config.h` is the persisted byte order; reordering shifts values).

- `/api/settings` (GET/POST JSON) — full config incl. mode.
- `/api/mode/<name>` and `/api/mode/random/{single,multi}` (GET|POST) — persisted mode switch.
- `/api/state` (GET) — current mode + live `lit` + API state.
- `/api/state` (POST JSON) and `/api/state/{off,solid/<c>,blink/<c>}` (GET|POST) —
  transient; **only in API mode** (409 otherwise). API idle = yellow blink.

Durations clamp to 100..600000 ms server-side.

## Conventions

- **Every `.cpp`/`.h` starts with the BullittSystems MIT header** (see existing files).
- **clang-format** (`.clang-format`, Google-based, 4-space, 100 col) is enforced in
  CI. Format before committing.
- Add library deps **per task**, as first used, not speculatively.
- Config (GPIO map, defaults, structs) lives in `config.h`.
- Embedded mindset: no dynamic allocation without cause; deterministic, non-blocking
  logic (`millis()`-based, rollover-safe; no `delay()` in the run loop).
- Public headers in `include/`, implementations in `src/`, host tests in `test/`
  (mocks under `test/mock/`).

## Testing

Host-native GoogleTest via `[env:native]`. Header-only mocks under `test/mock/`
(Arduino, esp_random, Preferences, freertos) resolve the hardware includes without
touching `src`. Not unit-tested (needs on-device / future curl integration):
`WifiProvisioner`, the mutex/concurrency, HTTP routing/409 end-to-end, the web page.
