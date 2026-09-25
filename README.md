# Traffic Light Controller

Firmware for a standard three-aspect **red / yellow / green** signal, running on an
ESP32 (a **Teyleten Robot ESP32-C3 SuperMini** by default, or a **D1 Mini ESP32**;
see [Supported boards](#supported-boards)). The device joins your Wi-Fi, serves a
small web UI, and exposes a REST API so it can run standalone patterns or be driven
as a status indicator by other systems.

## What it controls

The firmware's contract is intentionally simple and hardware-agnostic: **three GPIO
outputs, one per lamp, driven on/off** (active-high by default, see
[polarity](#output-polarity)). What those pins drive is the integrator's choice:
12V bulbs through MOSFETs, LEDs, a relay board, whatever. The firmware does not care
about voltage, current, or driver topology.

The **reference build** is a 3D-printed traffic light with three 12V LED bulbs
switched by low-side N-channel MOSFETs (originally a battery-powered Halloween
Trunk-or-Treat display, later a workshop indicator). Its wiring is documented in
[docs/traffic-light-controller-hardware.md](docs/traffic-light-controller-hardware.md),
but any driver meeting the pin contract works.

## Features

- **Modes:** sequence (normal green→yellow→red with configurable timings), random
  (single-lamp or multi-lamp, bounded random timing), chase (an overlapping
  R→R+Y→Y→Y+G→G→G+R walk), solid, blink, and off.
- **Overrides:** other systems can show any pattern temporarily on top of the
  mode, optionally for a limited time, without changing what it runs after a reboot.
- **Web UI** for selecting the mode, editing its settings, clearing an override, and
  managing Wi-Fi networks.
- **REST API** for automation, including webhook-friendly one-URL routes.
- **Wi-Fi provisioning** via a captive portal on first boot; no hardcoded creds.
- **Multiple networks:** up to 5 saved networks; the strongest one in range is joined.
- **Persistent settings:** the selected mode and its parameters survive reboots.
- Lamp behavior is fully independent of the network: it runs immediately on power-up.
- **Driver-agnostic:** three logic-level outputs with configurable polarity.

## Output polarity

By default the outputs are **active-high** (a HIGH pin turns its lamp on). For
active-low drivers (many relay boards, common-anode wiring), set
`LAMP_ACTIVE_HIGH = false` in [include/config.h](include/config.h) and rebuild;
`begin()` then holds the correct OFF level and on/off invert accordingly.

Note the boot window: before the firmware runs, the integrator's wiring should hold
the OFF level (the reference build uses pulldowns for its active-high MOSFETs; an
active-low driver would use pullups). `LampController::begin()` asserts OFF as early
as practical regardless.

The GPIO assignments live in [include/config.h](include/config.h) too
(`RED_GPIO` / `YELLOW_GPIO` / `GREEN_GPIO`).

## Getting started

### Prerequisites

- [PlatformIO](https://platformio.org/) (CLI or IDE).
- On **Apple Silicon**, install Rosetta 2 (`softwareupdate --install-rosetta
  --agree-to-license`). The ESP toolchain for this platform is x86_64-only; see
  [CLAUDE.md](CLAUDE.md) for details.

### Supported boards

Two targets, selected by PlatformIO environment:

| Board | Env | Notes |
|-------|-----|-------|
| Teyleten ESP32-C3 SuperMini | `esp32-c3-supermini` (default) | Tiny; native USB. Weak antenna: TX power is capped at 8.5 dBm (see below). |
| AITRIP/LOLIN D1 Mini ESP32 (ESP-WROOM-32) | `esp32-d1-mini` | Larger; stronger Wi-Fi; CP2104 UART. Only a RST button (see below). |

GPIO assignments and the BOOT button differ per board and are selected
automatically from the env's `TLC_BOARD_*` flag (see [include/config.h](include/config.h)):

- **C3:** lamps on GPIO3/4/5, BOOT button on GPIO9. Many C3 SuperMini units fail to
  associate with an access point at full TX power because of the board's antenna
  layout, so the firmware caps it at 8.5 dBm on this board.
- **D1 Mini ESP32:** lamps on GPIO22/21/17. The BOOT function is on GPIO0, but the
  board has no button for it: wire a momentary switch from **IO0 to GND** (the pin
  has an internal pullup).

### Build and flash

```sh
pio run                             # build default board (esp32-c3-supermini)
pio run -e esp32-d1-mini            # build the WROOM-32 board
pio run -e <env> -t upload          # flash over USB
pio device monitor                  # serial console at 115200
```

### First boot: Wi-Fi setup

On first boot (or when saved Wi-Fi is unavailable) the device starts an access
point named **`TrafficLight-Setup`**. Join it, and a captive portal lets you select
your network and enter the password. After it connects, the serial log prints the
device IP; the web UI is at `http://<device-ip>/`.

After a portal session the web UI can take up to ~2 minutes to come up (the log
shows `Web UI: port 80 busy, retrying` until then): the phone's closed connection
to the portal briefly holds port 80.

The selected mode runs the whole time, even before Wi-Fi is configured.

### Saved networks

The device keeps up to **5 networks**, in the order they were added. At boot (or
after losing its connection) it scans and joins the strongest saved network in
range. Joining a network never reorders or rewrites the list; it only changes when
you edit it.

If none are in range, the `TrafficLight-Setup` portal opens. A network entered
there is appended to the list. If the list is already full, nothing is evicted:
the new network is used as a **temporary** network until reboot, and the web UI (or
`POST /api/wifi/keep`) lets you save it, replacing one you pick.

### Changing networks / resetting Wi-Fi

- **Add or forget** networks from the web UI's Wi-Fi section or the
  [Wi-Fi API](#wi-fi-networks).
- **BOOT button**, held *while running* (holding it through reset enters the ROM
  bootloader instead):
  - **~5 s**: the lamps turn solid yellow. Release to restart into the portal with
    the saved networks kept, e.g. to add one at a new site.
  - **~15 s** (keep holding): all lamps flash, every saved network is forgotten,
    and the device restarts into the portal.

On the D1 Mini ESP32 this needs the external IO0 switch described under
[Supported boards](#supported-boards).

## Web UI

Browse to `http://<device-ip>/`. Pick a mode from the dropdown; the relevant
settings appear (sequence durations, random bounds + single/multiple, solid color,
blink color + interval, chase interval). Save persists the change and applies it
immediately, clearing any override. When an override is active, a line under the
form shows it (with the time left, if timed) and a Clear button.

The Wi-Fi section lists the saved networks (forget any), shows a temporary network
with a Keep option, adds networks, and can forget all networks and restart.

## REST API

All responses are JSON. Durations are in milliseconds and clamped to
100–600000 server-side.

The API has three verbs:

- **Mode** (`/api/mode/...`): what the light runs, now and after every reboot.
  Persisted.
- **Override** (`/api/override/...`): a temporary pattern on top of the mode, for
  other systems to drive the light. Never persisted; it lasts until cleared, until
  the mode is set, until its optional `ttlMs` expires, or until reboot.
- **State** (`GET /api/state`): read-only; what the light is doing right now.

Modes and overrides share one pattern vocabulary: `solid`, `blink`, `sequence`,
`random`, `chase`, `off`. Anything that sets the mode, including the web UI's Save,
clears the override.

For example: set the mode to chase, and it chases. Override to solid red, then to
blinking yellow; a reboot goes back to chase. Set the mode to solid yellow, and it
shows solid yellow now and after the next reboot.

### Settings (persisted, full config)

| Method | Path | Description |
|--------|------|-------------|
| GET | `/api/settings` | Current configuration |
| POST | `/api/settings` | Update any subset of settings |

```sh
curl http://<device-ip>/api/settings
curl -X POST http://<device-ip>/api/settings -H 'Content-Type: application/json' \
  -d '{"mode":"sequence","sequence":{"greenMs":5000,"yellowMs":2000,"redMs":5000}}'
```

A POST that includes `mode` clears any override; one without `mode` only updates
saved parameters (an active override keeps running).

### Patterns and parameters

Modes and overrides use the same patterns and parameters. Every parameter is
optional and falls back to the saved value; a pattern accepts only its own
parameters (anything else is a `400`).

| Pattern | Parameters |
|---------|------------|
| `solid` | `color` (`red`/`yellow`/`green`) |
| `blink` | `color`, `intervalMs` |
| `sequence` | `greenMs`, `yellowMs`, `redMs` |
| `random` | `variant` (`single`/`multi`), `minMs`, `maxMs` |
| `chase` | `intervalMs` |
| `off` | none |

On the one-URL routes the path names the pattern and the query string carries the
parameters, e.g. `/api/override/blink?color=red&intervalMs=200`. Durations must be
plain integers.

### Mode (persisted, GET or POST)

`/api/mode/<pattern>?<params>` saves the pattern **and** any parameters given, starts
it immediately, and clears any override.

```sh
curl http://<device-ip>/api/mode/chase
curl 'http://<device-ip>/api/mode/solid?color=yellow'
curl 'http://<device-ip>/api/mode/random?variant=multi&minMs=500&maxMs=3000'
```

### Override (transient)

An override is never saved: parameters sent with it apply to the override only.
It also takes `ttlMs`, clamped to 1000–86400000 (1 s to 24 h); without it, the
override lasts until cleared.

| Method | Path | Description |
|--------|------|-------------|
| GET/POST | `/api/override/<pattern>?<params>&ttlMs=` | One-URL form |
| POST | `/api/override` | JSON: `{"pattern", ...params, "ttlMs"?}` |
| GET/POST | `/api/override/clear` | Back to the mode |
| DELETE | `/api/override` | Back to the mode |

```sh
curl 'http://<device-ip>/api/override/blink?color=red'
curl 'http://<device-ip>/api/override/chase?intervalMs=150&ttlMs=60000'  # a minute
curl -X POST http://<device-ip>/api/override -H 'Content-Type: application/json' \
  -d '{"pattern":"blink","color":"yellow","intervalMs":200,"ttlMs":30000}'
curl http://<device-ip>/api/override/clear
```

For a heartbeat-style indicator, have the driving system re-send its override with
a `ttlMs` longer than its send interval; if it stops, the light falls back to the
mode on its own.

### State (read-only)

| Method | Path | Description |
|--------|------|-------------|
| GET | `/api/state` | Saved mode, active pattern, lit lamp(s), and the override |

Every mode and override route also responds with this body. `active` is the
pattern actually running (the override's if there is one); `lit` is `off`, `red`,
`yellow`, `green`, or `multi`. `override` is `null` when none is active, and
`ttlMs`/`remainingMs` appear only on a timed override.

```json
{ "mode": "chase", "active": "blink", "lit": "red",
  "override": { "pattern": "blink", "color": "red", "intervalMs": 200,
                "ttlMs": 60000, "remainingMs": 42000 } }
```

### Wi-Fi networks

Passwords are write-only: no response ever includes one. POST routes take a JSON
body (`Content-Type: application/json`).

| Method | Path | Body | Description |
|--------|------|------|-------------|
| GET | `/api/wifi` | | Saved SSIDs, temporary (`session`) and `connected` SSID |
| POST | `/api/wifi/add` | `{"ssid","password"?}` | Save a network (409 if full) |
| POST | `/api/wifi/forget` | `{"ssid"}` | Remove a saved network (404 if unknown) |
| POST | `/api/wifi/keep` | `{"replace"?}` | Save the temporary network; `replace` is required when full |
| POST | `/api/wifi/reset` | | Forget all networks and restart into the portal (202) |

```sh
curl http://<device-ip>/api/wifi
curl -X POST http://<device-ip>/api/wifi/add -H 'Content-Type: application/json' \
  -d '{"ssid":"Work","password":"hunter22"}'
curl -X POST http://<device-ip>/api/wifi/keep -H 'Content-Type: application/json' \
  -d '{"replace":"OldCafe"}'
```

```json
{ "networks": [{ "ssid": "Home" }, { "ssid": "Work" }],
  "session": null, "connected": "Home", "max": 5 }
```

## Testing

Host-native unit tests (GoogleTest) cover the hardware-independent logic:

```sh
pio test -e native
```

They run on your machine with header-only hardware mocks; no board required.

[scripts/api-test.sh](scripts/api-test.sh) is an integration test against a running
device. It needs only bash and curl, and exercises the settings, mode, override
(including TTL expiry), state, and Wi-Fi routes:

```sh
scripts/api-test.sh <device-ip>
```

It changes the device's mode and settings while it runs and restores the original
settings on exit. Wi-Fi checks are non-destructive: a throwaway network is added and
forgotten only if the list has room, and `/api/wifi/reset` is never called.

## Project structure

```
include/     public headers (config, modes, controller, codec, web, wifi, version)
src/         implementations + main.cpp
test/        native GoogleTest suites; test/mock/ holds header-only mocks
docs/        hardware documentation
scripts/     api-test.sh (curl integration test against a device)
```

Architecture and conventions are described in [CLAUDE.md](CLAUDE.md).

## License

MIT. See [LICENSE](LICENSE).
