# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [0.0.1] - unreleased

Initial firmware for the ESP32-C3 traffic light controller.

### Added
- **Lamp control**: three logic-level GPIO channels (red/yellow/green), driver-
  agnostic with configurable polarity (`LAMP_ACTIVE_HIGH`, active-high default) and
  boot-safe initialization (all lamps driven to OFF before anything else).
- **Modes**: `sequence` (configurable green/yellow/red durations), `random`
  (single lamp or multi-lamp subsets, bounded random hold times), `solid`,
  `blink`, `off`, and `api`. All persisted to NVS and auto-resumed on boot.
- **StepMode engine**: data-driven, non-blocking (`millis()`-based, rollover-safe)
  pattern engine backing the table modes; supports multi-lamp steps.
- **Wi-Fi provisioning**: non-blocking WiFiManager captive portal
  (`TrafficLight-Setup`); lamp sequencing runs regardless of network state. Holding
  the BOOT button ~5s clears saved credentials and reopens the portal.
- **Web UI**: embedded page to select mode and edit per-mode settings.
- **REST API**:
  - `/api/settings` (GET/POST) - full configuration.
  - `/api/mode/<name>` and `/api/mode/random/{single,multi}` - persisted mode switch.
  - `/api/state` (GET/POST) and webhook path routes - transient control while in
    API mode (409 otherwise); API idle shows a yellow "ready" blink.
- **Persistence**: all settings stored in NVS with validity guards on load.
- **Tests**: host-native GoogleTest suite covering LampController, StepMode,
  RandomMode, ModeController, Settings, and SettingsCodec.
- **CI**: GitHub Actions building the firmware, running unit tests, and enforcing
  clang-format.
