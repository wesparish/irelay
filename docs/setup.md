# Setup Guide

## Prerequisites

- [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html) (`pip install platformio`)
- Two [HiLetgo ESP8285 ESP-01M IR Transceiver modules](https://www.amazon.com/dp/B09KGXNZ2Q)
- A programmer: [DSD TECH SH-U09C5 USB to TTL UART converter cable (FTDI)](https://www.amazon.com/gp/product/B07WX2DSVB/) — requires jumper wires (see wiring diagram in `docs/ft232rl-programmer-datasheet.md`)
- Enclosure candidate: https://www.printables.com/model/810034-case-for-esp8285-esp-01m-ir-infrared-transmitter-m

## Programmer Setup

See `docs/ft232rl-programmer-datasheet.md` for the full wiring diagram. Every time you flash (steps 1 and 3):

1. Set the FT232RNL voltage selection to **5V**
2. Wire GND→GND, 5V→5V, TX→RX, RX→TX
3. Add a jumper from **IO0 to GND** before power-cycling into flash mode
4. Remove the IO0 jumper and power-cycle after flashing to boot normally

## Step 1 — Get the MAC address of each module

You need the MAC address of each ESP8285 before you can configure them to talk to each other. Flash the `get-mac` utility to each module one at a time:

```bash
./bin/get-mac [port]
```

The MAC address will print to the serial console and repeat every 5 seconds. Note both MACs — label them "receiver" and "emitter" to avoid mixing them up.

If PlatformIO doesn't auto-detect the port, pass it explicitly:

```bash
# Linux
./bin/get-mac /dev/ttyUSB0

# macOS
./bin/get-mac /dev/cu.usbserial-0001
```

To list connected serial ports:

```bash
pio device list
```

## Step 2 — Configure

One `include/config.h` covers both nodes. Create it from the example:

```bash
cp include/config.h.example include/config.h
```

Edit `include/config.h` and set:

- `ESPNOW_CHANNEL` — any channel 1–13; both nodes must use the **same** value
- `RECEIVER_NODE_MAC` — the MAC of the receiver-node (from step 1)
- `EMITTER_NODE_MAC` — the MAC of the emitter-node (from step 1)

```c
#define ESPNOW_CHANNEL    1
#define RECEIVER_NODE_MAC {0x5C, 0xCF, 0x7F, 0x12, 0x34, 0x56}
#define EMITTER_NODE_MAC  {0x5C, 0xCF, 0x7F, 0xAB, 0xCD, 0xEF}
```

The firmware picks the correct peer MAC at compile time based on which role is being built — no need to edit config between flashes.

## Step 3 — Flash each node

```bash
./bin/flash receiver-node [port]
./bin/flash emitter-node [port]
```

`bin/flash` validates that `config.h` exists and that the placeholder MAC has been replaced before flashing.

## Step 4 — Verify

Open the serial monitor on either node:

```bash
./bin/monitor [port]
```

On startup you should see:
```
receiver-node ready
```
or
```
emitter-node ready
```

Point a remote at the receiver-node (IR receiver side) and press a button. The emitter-node will retransmit the IR signal immediately. Use an IR receiver or your target device to confirm.

## Putting it in production

- Mount the **receiver-node** near the remote's point of use with the IR receiver facing the seating area
- Mount the **emitter-node** in front of the target device with the IR LED aimed at the device's receiver window
- Both modules run off 5V and draw very little current — a small USB power supply works well
