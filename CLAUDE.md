# CLAUDE.md

Project-specific context for the Traffic Light Controller firmware. General
working-style preferences live in the user's global config; this file holds facts
about *this* repo.

## What this is

Firmware for a standard three-aspect **red/yellow/green** signal on an ESP32 (Teyleten
ESP32-C3 SuperMini by default, or a D1 Mini ESP32). The contract is hardware-agnostic: **three GPIO outputs,
one per lamp, driven on/off** (active-high by default; `LAMP_ACTIVE_HIGH` in
`config.h` flips polarity for active-low drivers). Voltage/current/driver topology
are the integrator's concern, not the firmware's. The device joins Wi-Fi, serves a
web UI, and exposes a REST API. The **reference build** (12V bulbs + N-channel
MOSFETs) is documented in
[docs/traffic-light-controller-hardware.md](docs/traffic-light-controller-hardware.md).

## Build / flash / test

PlatformIO. On this dev machine `pio` is at `~/.platformio/penv/bin/pio` (not on PATH).

Two firmware envs share `[common]` in platformio.ini and select GPIOs/reset-button
via a `TLC_BOARD_*` build flag consumed in `config.h`:
- `esp32-c3-supermini` (default, `-DTLC_BOARD_C3`): lamps GPIO3/4/5, BOOT GPIO9, native
  USB CDC. `WifiProvisioner::begin()` caps TX power at 8.5 dBm under `TLC_BOARD_C3`:
  at full power these boards routinely fail to associate (antenna layout). Verified
  on three units; do not remove.
- `esp32-d1-mini` (`-DTLC_BOARD_ESP32`): LOLIN/AITRIP D1 Mini ESP32 (WROOM-32), lamps
  GPIO22/21/17, CP2104 UART. Stronger Wi-Fi than the C3 SuperMini. BOOT function on
  GPIO0, but the board has only RST, so it needs an external IO0-to-GND switch.

- Build: `pio run` (default C3) or `pio run -e esp32-d1-mini`
- Flash: `pio run -e <env> -t upload`
- Serial monitor: `pio device monitor` (115200)
- Unit tests (host): `pio test -e native` (board-agnostic; builds with the C3 default pins)

### Toolchain gotcha (Apple Silicon)

The `espressif32` platform ships precompiled IDF archives built with an
**x86_64-only gcc 8.4** toolchain. On Apple Silicon it runs under **Rosetta 2**
(`softwareupdate --install-rosetta`). Do **not** pin a newer native toolchain:
gcc 15 fails to assemble the core (`zicsr`) and ABI-mismatches the precompiled
archives. CI runs on x86_64 Linux, so it builds natively there.

## Architecture

Decoupled layers; lamp behavior never depends on the network.

- `LampController`: owns the 3 GPIOs; boot-safe `begin()` drives all to the OFF
  level first. Polarity configurable (`activeHigh` ctor arg, default `LAMP_ACTIVE_HIGH`).
- `StepMode`: generic engine that walks a `Step{lampMask, durationMs}` table
  (`durationMs==0` = hold forever). Backs Sequence/Solid/Blink/Chase/Off.
- `RandomMode`: algorithmic; single (one lamp, never repeats) or multi (random
  non-empty subset, always changes); uses `esp_random()`.
- `ModeController`: owns what the lamps show: the persisted mode (baseline) plus an
  optional transient `Override` (an `AppSettings` copy whose `mode` is the pattern,
  and a `ttlMs`, checked in `tick()`). Everything runs off `active()` (override if
  set, else baseline), so every pattern works as an override. `apply(s, clearOverride)`
  / `setOverride()` / `clearOverride()` / `status()`. **Guarded by a FreeRTOS mutex**
  because web handlers and `loop()` run on different tasks.
- `Settings`: persists `AppSettings` to NVS (Preferences).
- `SettingsCodec`: pure JSON parse/validate/serialize (no web-server dep, unit-tested).
- `WifiCodec`: same idea for `/api/wifi` (shape checks only; content validated by
  `KnownNetworks`). Never serializes passwords.
- `WebInterface`: ESPAsyncWebServer on :80; thin handlers delegate to the codecs.
- `KnownNetworks`: up to 5 saved Wi-Fi networks in NVS namespace `tlwifi`, in
  stable insertion order. **Never reordered or auto-evicted**: connecting writes
  nothing; the list is only written when edited (add/replace/forget). A full list
  returns `Full`. Fixed storage, no Wi-Fi deps, unit-tested.
- `WifiProvisioner`: non-blocking state machine: async scan → connect to the
  strongest known network in range → else WiFiManager portal `TrafficLight-Setup`.
  Portal saves are **appended** to `KnownNetworks`; if the list is full, the network
  becomes a RAM-only **session network** (used until reboot, also a rescan
  candidate) that the user can persist via `keepSessionNetwork(replaceSsid)`. `WiFi.persistent
  (false)`: the list is the only store (a pre-existing core-saved network is migrated
  once, then the core slot is wiped). `resetCredentials()` clears the list. List and
  session access is **mutex-guarded** (web handlers vs `loop()`). The `WiFiManager`
  instance is file-local to the .cpp: its headers define `HTTP_GET`/`HTTP_POST`,
  which clash with ESPAsyncWebServer's, so never include `WiFiManager.h` from a header.
- BOOT button (`RESET_BUTTON_GPIO`), two tiers, polled in `loop()` (not at boot: the
  pin low at reset enters the bootloader). Held past `WIFI_PORTAL_HOLD_MS` the lamps
  go solid yellow and the modes pause; release restarts into the portal with the
  list kept (`requestPortalOnBoot()`, an RTC_NOINIT magic, no flash write). Held to
  `WIFI_RESET_HOLD_MS` it forgets all and restarts. The portal can't open in place
  once the web server owns :80, hence the restart.

Boot flow (`main.cpp`): lamps OFF -> load settings -> `controller.apply()` (starts
mode immediately) -> Wi-Fi in background -> web server once connected. The bind is
retried every `WEB_BIND_RETRY_MS`: after a portal session the phone's connection to
:80 sits in TCP TIME_WAIT (up to ~2 min) and lwIP refuses the port until it clears.

### Concurrency

ESPAsyncWebServer callbacks run on the **AsyncTCP task**, not `loop()`. Any shared
mode/lamp state a handler touches must go through `ModeController`, and Wi-Fi list
state through `WifiProvisioner`; both serialize with a mutex. Do not mutate
`StepMode`/`RandomMode`/`KnownNetworks` directly from a handler. Anything that must
run on the loop task (e.g. the `/api/wifi/reset` restart) is signalled with a flag.

### Routing gotcha

ESPAsyncWebServer matches URIs by **prefix in registration order**. Register
specific routes before generic ones or the generic one shadows them: e.g. all
`/api/override/...` routes before the JSON `POST /api/override` handler, and
`/api/wifi/...` before `GET /api/wifi`. (Precisely: a plain string route matches
`^uri(/.*)?$`.) The mode/override pattern routes use `AsyncURIMatcher::exact()`, so
`/api/mode/blink/red` is a 404 rather than blink.

## Modes and API

Modes (persisted): `off`, `solid`, `blink`, `sequence`, `random`, `chase`. The enum
order in `config.h` is the persisted byte order: `Off` is 0, new modes are appended
and `MODE_LAST` moved to match (it bounds the `Settings::load` range guard).
User-facing lists (UI dropdown, route tables, docs) still show Off last. The same names
are the override pattern vocabulary.

Three verbs:
- **Mode** `/api/mode/<pattern>?<params>` (GET|POST): persist the pattern and any
  params given, run it, clear any override.
- **Override** `/api/override/<pattern>?<params>&ttlMs=` (GET|POST), `POST
  /api/override` (JSON `{"pattern", ...flat params, "ttlMs"?}`), `/api/override/clear`
  (GET|POST), `DELETE /api/override`. Transient, never persisted; starts from the
  saved settings. `ttlMs` clamps to 1 s..24 h.

Path = pattern, query = params, and the query goes through the same parser as the
JSON body: `SettingsCodec::putQueryParam` turns digit-only values into numbers, then
`parseOverride`/`parseModeCommand` validate. Strict: each pattern accepts only its own
params (unknown or foreign keys are 400, `ttlMs` on a mode route is 400).
- **State** `GET /api/state`: read-only `{mode, active, lit, override|null}`. Every
  mode/override route responds with it.

Rule: any request that sets the mode clears the override, including a `/api/settings`
POST containing `mode` (the web UI's Save always does). A settings POST without
`mode` updates the baseline only; the override keeps running.

- `/api/settings` (GET/POST JSON): full config incl. mode.
- `/api/wifi` (GET) lists saved SSIDs, the session network, and the connected SSID.
  POST JSON `/api/wifi/{add,forget,keep}` edit the list (409 full, 404 unknown, 400
  invalid). POST `/api/wifi/reset` returns 202; `loop()` then forgets all and restarts.

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
touching `src`. Not unit-tested: `WifiProvisioner`, the mutex/concurrency, the web
page. HTTP routing end-to-end is covered by `scripts/api-test.sh <device-ip>` (bash +
curl, run against a flashed device; restores settings on exit). Update it when
routes change.
