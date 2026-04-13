# Architecture

## System Overview

```
  Room A                                  Room B
  ──────────────────────────────────────────────────────
  [Remote] ──IR──► [receiver-node]  ···ESP-NOW···  [emitter-node] ──IR──► [Device]
                   ESP8285                          ESP8285
                   GPIO14 (RX)                      GPIO4 (TX)
```

The two modules never join a WiFi network. They communicate directly over **ESP-NOW**, Espressif's peer-to-peer MAC-layer protocol, using each other's MAC address. There is no router, broker, or infrastructure dependency.

## ESP-NOW

ESP-NOW operates at the 802.11 MAC layer. After `esp_now_init()` and `wifi_set_channel()`, both devices can exchange up to 250-byte payloads with ~1ms latency using a simple callback model. The receiver-node is initialized as `ESP_NOW_ROLE_CONTROLLER`; the emitter-node as `ESP_NOW_ROLE_SLAVE`.

Both nodes call `WiFi.mode(WIFI_STA)` to initialize the radio but never call `WiFi.begin()`. The channel is fixed at compile time via `ESPNOW_CHANNEL` in `config.h`.

## IR Payload

The decoded IR signal is packed into a 16-byte struct (uint32_t + uint16_t + 2-byte padding + uint64_t):

```cpp
struct IrPayload {
    uint32_t protocol;  // IRremoteESP8266 decode_type_t
    uint16_t bits;      // signal bit length
    uint64_t value;     // decoded IR value
};
```

IRremoteESP8266 supports hundreds of protocols. The protocol type is forwarded as-is, so the emitter-node can call `irSend.send()` with the exact same type/value pair — no protocol-specific handling needed in application code.

## Compile-Time Role Selection

Both `receiver-node` and `emitter-node` build from the same `src/main.cpp`. The active code path is selected at compile time via the `NODE_ROLE` build flag:

```ini
# platformio.ini
[env:receiver-node]
build_flags = -D NODE_ROLE=ROLE_RECEIVER

[env:emitter-node]
build_flags = -D NODE_ROLE=ROLE_EMITTER
```

`#if NODE_ROLE == ROLE_RECEIVER` / `#elif NODE_ROLE == ROLE_EMITTER` guards in `main.cpp` ensure only the relevant code is compiled into each binary. A `#error` directive fires at compile time if `NODE_ROLE` is missing.

## Pin Assignments

| Pin    | GPIO | Role |
|--------|------|------|
| GPIO14 | 14   | IR receive (receiver-node) |
| GPIO4  | 4    | IR transmit (emitter-node) |

These are hardwired on the ESP-01M IR transceiver module PCB — confirmed by the manufacturer's product documentation. They are not configurable without hardware modification.

## Web UI

Each node runs a lightweight HTTP server (`ESP8266WebServer`) and a captive DNS server (`DNSServer`) alongside the main firmware loop. On boot:

1. The radio is set to `WIFI_AP_STA` mode so both the ESP-NOW STA interface and the AP coexist on the same channel (`ESPNOW_CHANNEL`).
2. An open access point is advertised as `IRelay-Receiver-XXXX` or `IRelay-Emitter-XXXX` (last 4 hex digits of the MAC).
3. Any DNS query is resolved to `192.168.4.1` (captive portal), and all non-`/` HTTP requests redirect there.
4. `GET /` returns a self-contained dark-mode HTML page (auto-refresh every 5 s) with live metrics and a 30-entry event log ring buffer.

RAM cost of the log buffer: 30 × 80 bytes = 2.4 KB (static BSS). The HTML is built with chunked transfer via `sendContent()` to avoid a single large heap allocation.

## CI/CD

- **Every push/PR**: `build.yml` compiles both environments and uploads `.bin` artifacts
- **Push to `main`**: `release.yml` runs semantic-release, which reads conventional commits, bumps the version, generates `CHANGELOG.md`, and attaches both firmware binaries to a GitHub release

Firmware binaries are attached to releases as:
- `firmware-receiver-node.bin`
- `firmware-emitter-node.bin`
