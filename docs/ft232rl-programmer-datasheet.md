# DSD TECH SH-U09C5 USB to TTL UART Converter Cable (FT232RNL)

Product: [DSD TECH SH-U09C5 USB to TTL UART Converter Cable (FTDI)](https://www.amazon.com/gp/product/B07WX2DSVB/)

## Overview

USB-to-UART cable using the FTDI FT232RNL IC. Used to flash firmware onto the ESP8285 ESP-01M IR transceiver modules via jumper wires. Requires manual IO0→GND wiring to enter download mode (no auto-PROG switch).

## Electrical Specs

| Parameter | Value |
|-----------|-------|
| USB-to-UART IC | FT232RNL |
| Output voltage | **1.8V / 2.5V / 3.3V / 5V** — selectable via VCC pin |
| Logic level | matches selected voltage |

> **Select the 5V VCC pin** before connecting to the ESP8285 ESP-01M. The module has an onboard 5V-to-3.3V regulator and requires 5V input. Supplying 3.3V will undervolt the ESP8285 through the regulator's dropout.

## Pins Used for ESP8285 Programming

| FT232RNL pin | Direction | ESP8285 ESP-01M pin |
|-------------|-----------|---------------------|
| 5V | → | 5V (VCC) |
| GND | → | GND |
| TXD | → | RXD (IO19) |
| RXD | ← | TXD (IO20) |
| GND | → | IO0 *(flash mode only — see below)* |

## Wiring Diagram

```
FT232RNL cable           ESP8285 ESP-01M
┌─────────────┐          ┌──────────────┐
│          5V ├──────────┤ 5V           │
│         GND ├────┬─────┤ GND          │
│         TXD ├────┼────►│ RXD (IO19)   │
│         RXD │◄───┼─────┤ TXD (IO20)   │
└─────────────┘    │     │              │
                   └─────┤ IO0 (GPIO0)  │  ← flash mode only;
                         └──────────────┘    remove after flashing
```

## Flash Mode Procedure

1. Connect IO0 to GND (add the jumper shown above)
2. Power-cycle the module (disconnect and reconnect USB)
3. Flash firmware: `./bin/flash <receiver-node|emitter-node> [port]`
4. Remove the IO0–GND jumper
5. Power-cycle again — module boots normally

## Finding the Serial Port

```bash
pio device list
```

Typical ports:
- Linux: `/dev/ttyUSB0`
- macOS: `/dev/cu.usbserial-XXXXXX`
