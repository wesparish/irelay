# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**IRelay** — wireless IR extender using two ESP8285 (ESP-01M) modules with integrated IR transceivers. One **receiver-node** sits in the media room near the projector and receives IR signals from the AVR remote. It relays them over ESP-NOW to the **emitter-node**, which re-transmits the IR signal in front of the AVR receiver in the server room. The result is a seamless remote control experience with the AVR physically relocated away from the media room.

**Hardware**: [HiLetgo ESP8285 ESP-01M IR Transceiver Module](https://www.amazon.com/HiLetgo-Infrared-Transmitter-Receiver-Transceiver/dp/B09KGXNZ2Q/)
**Enclosure candidate**: https://www.printables.com/model/810034-case-for-esp8285-esp-01m-ir-infrared-transmitter-m/files

## Common Commands

**Helper scripts** (run from project root, always prefer these over raw pio commands for device operations):

```bash
./bin/get-mac [port]                      # flash MAC-printer and read output
./bin/flash <receiver-node|emitter-node> [port]   # validate config, build, flash
./bin/monitor [port]                      # serial monitor at 115200 baud
```

**PlatformIO directly:**

```bash
pio run                               # build all environments
pio run -e emitter-node               # build one environment (or receiver-node)
pio run -e get-mac --target upload    # flash MAC utility (what bin/get-mac wraps)
pio run --target clean
pio test -e native                    # host-side unit tests (run after any changes to src/, include/, or test/)
pio device list                       # list available serial ports
```

## Architecture

### Two-Role Firmware from One Codebase

Both nodes share the same codebase. The role is set at compile time via a build flag in `platformio.ini`:

- `receiver-node`: `-D NODE_ROLE=ROLE_RECEIVER` — enables IR receive, disables IR transmit
- `emitter-node`: `-D NODE_ROLE=ROLE_EMITTER` — enables IR transmit, disables IR receive

`src/main.cpp` reads `NODE_ROLE` at compile time and initializes the appropriate path.

### Communication

Nodes communicate via **ESP-NOW** (`espnow.h`) — Espressif's peer-to-peer MAC-layer protocol. No router or broker required; the two devices talk directly using each other's MAC address.

Both nodes call `WiFi.mode(WIFI_STA)` and `wifi_set_channel()` but never join a network. The receiver-node is `ESP_NOW_ROLE_CONTROLLER`; the emitter-node is `ESP_NOW_ROLE_SLAVE`.

The MAC address of the peer and the shared channel are set in `include/config.h` (not committed — copied from `include/config.h.example`). To find a device's MAC, flash a sketch that prints `WiFi.macAddress()` before deploying real firmware.

### IR Handling

Uses the `IRremoteESP8266` library. Pin assignments are hardwired on the module PCB per the manufacturer's spec:
- **IR Receive**: GPIO14
- **IR Transmit**: GPIO4

### Directory Structure

```
src/           Main firmware source (shared between both roles)
include/       Headers; config.h (gitignored), config.h.example (committed)
test/          PlatformIO native unit tests
.github/
  workflows/
    ci.yml         CI/CD: test → build → release (release job only on main)
.releaserc.json    semantic-release config (conventional commits)
```

## Versioning & CI/CD

Commits must follow the [Conventional Commits](https://www.conventionalcommits.org/) spec — semantic-release uses this to determine version bumps and generate `CHANGELOG.md`.

| Commit prefix | Release type |
|---|---|
| `fix:` | patch |
| `feat:` | minor |
| `feat!:` / `BREAKING CHANGE:` | major |
| `chore:`, `docs:`, `refactor:` | no release |

GitHub Actions (`ci.yml`) runs on every push with three jobs:
1. **`test`** — runs `pio test -e native` (host-side unit tests)
2. **`build`** — builds both `receiver-node` and `emitter-node` firmware, uploads `.bin` artifacts
3. **`release`** — runs only on `main` after `test` and `build` pass; downloads the artifacts from `build` and invokes semantic-release to tag, generate changelog, and attach firmware binaries to the GitHub release

The `GITHUB_TOKEN` secret is automatically available in Actions; no extra secrets needed beyond WiFi/MQTT credentials (which never go into CI).
