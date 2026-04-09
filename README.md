# esp8285-wireless-ir-extender

> **Note:** This project is untested — hardware has not been ordered or assembled yet. Expect rough edges.

Relay IR remote commands over ESP-NOW between two rooms — no router, no broker, ~1ms latency.

One **media-node** sits in the media room and captures IR from your remote. It forwards the signal wirelessly to the **server-node** in your server room, which re-transmits it to the AVR receiver. The result is a seamless remote control experience with the AVR physically relocated.

**Hardware:** [HiLetgo ESP8285 ESP-01M IR Transceiver](https://www.amazon.com/dp/B09KGXNZ2Q) × 2  
**Programmer:** [DSD TECH SH-U09C5 USB to TTL UART Converter Cable (FTDI)](https://www.amazon.com/gp/product/B07WX2DSVB/) (5V — required for this module; requires manual wiring)

---

## Quickstart

### 1. Get each module's MAC address

Plug in each ESP8285 one at a time and run:

```bash
./bin/get-mac [port]
```

Note the MAC for each device.

### 2. Configure

```bash
cp include/config.h.example include/config.h
```

Edit `include/config.h` — set `ESPNOW_CHANNEL` (any 1–13, same on both nodes), `MEDIA_NODE_MAC`, and `SERVER_NODE_MAC`. The firmware selects the correct peer at compile time; one config file covers both flashes.

### 3. Flash

```bash
./bin/flash media-node   # the module going in the media room
./bin/flash server-node  # the module going in front of the AVR
```

That's it. Point your remote at the media-node and the server-node will retransmit.

---

## Docs

- [Setup Guide](docs/setup.md) — full walkthrough including wiring and verification
- [Architecture](docs/architecture.md) — ESP-NOW design, payload format, pin rationale, CI/CD
- [bin/ Tools Reference](docs/bin-tools.md) — `get-mac`, `flash`, `monitor`
