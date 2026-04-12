# bin/ Tools Reference

All scripts are in `bin/` and should be run from the project root. Make them executable once after cloning:

```bash
chmod +x bin/*
```

---

## bin/get-mac

Flashes a minimal firmware to a connected ESP8285 that prints its MAC address over serial, then opens the monitor so you can read it.

```
Usage: ./bin/get-mac [port]
```

| Argument | Description |
|----------|-------------|
| `port`   | Optional. Serial port (e.g. `/dev/ttyUSB0`, `/dev/cu.usbserial-0001`). Auto-detected if omitted. |

**Example:**
```bash
./bin/get-mac
./bin/get-mac /dev/ttyUSB0
```

Press **Ctrl+C** once you have the MAC. Repeat for the second module. You'll need both MACs to configure `include/config.h`.

---

## bin/flash

Builds and flashes the firmware for a named node role. Validates that `include/config.h` exists and that the placeholder MAC has been replaced before attempting to flash.

```
Usage: ./bin/flash <receiver-node|emitter-node> [port]
```

| Argument | Description |
|----------|-------------|
| `receiver-node` \| `emitter-node` | Required. Which role to flash. |
| `port` | Optional. Serial port. Auto-detected if omitted. |

**Example:**
```bash
./bin/flash receiver-node
./bin/flash emitter-node /dev/ttyUSB0
```

If `config.h` is missing or still contains the example placeholder MAC (`0xAA, 0xBB, ...`), the script exits with a clear error before touching the device.

---

## bin/monitor

Opens a serial monitor at 115200 baud.

```
Usage: ./bin/monitor [port]
```

**Example:**
```bash
./bin/monitor
./bin/monitor /dev/ttyUSB0
```

Press **Ctrl+C** to exit. To list available ports: `pio device list`.
