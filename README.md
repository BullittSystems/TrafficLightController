# Traffic Light Controller

Firmware for a standard three-aspect **red / yellow / green** signal, running on a
**Teyleten Robot ESP32-C3 SuperMini**. The device joins your Wi-Fi, serves a small
web UI, and exposes a REST API so it can run standalone patterns or be driven as a
status indicator by other systems.

## What it controls

The firmware's contract is intentionally simple and hardware-agnostic: **three GPIO
outputs, one per lamp, driven on/off** (active-high by default, see
[polarity](#output-polarity)). What those pins drive is the integrator's choice —
12V bulbs through MOSFETs, LEDs, a relay board, whatever. The firmware does not care
about voltage, current, or driver topology.

The **reference build** is a 3D-printed traffic light with three 12V LED bulbs
switched by low-side N-channel MOSFETs (originally a battery-powered Halloween
Trunk-or-Treat display, later a workshop indicator). Its wiring is documented in
[docs/traffic-light-controller-hardware.md](docs/traffic-light-controller-hardware.md),
but any driver meeting the pin contract works.

## Features

- **Modes:** sequence (normal green→yellow→red with configurable timings), random
  (single-lamp or multi-lamp, bounded random timing), solid, blink, off, and an
  API-driven mode.
- **Web UI** for selecting the mode and editing its settings.
- **REST API** for automation, including webhook-friendly one-URL routes.
- **Wi-Fi provisioning** via a captive portal on first boot; no hardcoded creds.
- **Persistent settings** — the selected mode and its parameters survive reboots.
- Lamp behavior is fully independent of the network: it runs immediately on power-up.
- **Driver-agnostic:** three logic-level outputs with configurable polarity.

## Output polarity

By default the outputs are **active-high** (a HIGH pin turns its lamp on). For
active-low drivers — many relay boards, common-anode wiring — set
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

### Build and flash

```sh
pio run                 # build firmware (default env: esp32-c3-supermini)
pio run -t upload       # flash over USB
pio device monitor      # serial console at 115200
```

### First boot: Wi-Fi setup

On first boot (or when saved Wi-Fi is unavailable) the device starts an access
point named **`TrafficLight-Setup`**. Join it, and a captive portal lets you select
your network and enter the password. After it connects, the serial log prints the
device IP; the web UI is at `http://<device-ip>/`.

Lamp sequencing runs the whole time, even before Wi-Fi is configured.

### Changing networks / resetting Wi-Fi

To move the device to a different network, clear its saved credentials: with the
firmware running, **hold the BOOT button for ~5 seconds**. All three lamps flash to
acknowledge, then it restarts into the `TrafficLight-Setup` portal so you can pick a
new network. (Hold it *while running*, not through reset — holding BOOT during reset
just enters the ROM bootloader.)

## Web UI

Browse to `http://<device-ip>/`. Pick a mode from the dropdown; the relevant
settings appear (sequence durations, random bounds + single/multiple, solid color,
blink color + interval). Save persists the change and applies it immediately.

## REST API

All responses are JSON. Durations are in milliseconds and clamped to
100–600000 server-side.

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

### Mode switching (persisted, GET or POST)

| Path | Description |
|------|-------------|
| `/api/mode/sequence` | Standard green→yellow→red cycle |
| `/api/mode/random` | Random, using the saved single/multi variant |
| `/api/mode/random/single` | Random, one lamp at a time |
| `/api/mode/random/multi` | Random, multi-lamp subsets |
| `/api/mode/solid` | Hold the saved solid color |
| `/api/mode/blink` | Blink the saved blink color |
| `/api/mode/off` | All lamps off |
| `/api/mode/api` | Hand control to the API (see below) |

```sh
curl http://<device-ip>/api/mode/random/multi
```

### State (transient, API mode only)

`/api/state` writes are honored **only while in API mode** (`/api/mode/api`);
otherwise they return `409`. State is transient — on reboot the device returns to
API mode idle (a yellow "ready" blink) until the next command. This is the path for
indicator use: an external system asserts the current state.

| Method | Path | Description |
|--------|------|-------------|
| GET | `/api/state` | Current mode, live lit lamp(s), and API state |
| POST | `/api/state` | Set state via JSON (`off` / `solid` / `blink` + color) |
| GET/POST | `/api/state/off` | All off |
| GET/POST | `/api/state/solid/<color>` | Solid red/yellow/green |
| GET/POST | `/api/state/blink/<color>` | Blink red/yellow/green |

```sh
curl http://<device-ip>/api/mode/api
curl http://<device-ip>/api/state/solid/green
curl -X POST http://<device-ip>/api/state -H 'Content-Type: application/json' \
  -d '{"state":"blink","color":"yellow","intervalMs":300}'
curl http://<device-ip>/api/state          # read current state
```

A `GET /api/state` response:

```json
{ "mode": "api", "lit": "green",
  "state": { "state": "solid", "color": "green", "intervalMs": 500 } }
```

## Testing

Host-native unit tests (GoogleTest) cover the hardware-independent logic:

```sh
pio test -e native
```

They run on your machine with header-only hardware mocks — no board required.

## Project structure

```
include/     public headers (config, modes, controller, codec, web, wifi, version)
src/         implementations + main.cpp
test/        native GoogleTest suites; test/mock/ holds header-only mocks
docs/        hardware documentation
```

Architecture and conventions are described in [CLAUDE.md](CLAUDE.md).

## License

MIT. See [LICENSE](LICENSE).
