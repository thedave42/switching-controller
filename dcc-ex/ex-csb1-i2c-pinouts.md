# EX-CSB1 I2C Connector Pinouts

Pin assignments derived from the EX-CSB1 KiCad schematic (`EX-CSB1.kicad_sch`) in the [DCC-EX/EX-CSB1](https://github.com/DCC-EX/EX-CSB1) repository.

All three I2C connectors share the same SDA/SCL bus and are powered from the +3.3V rail.

---

## QWiic / STEMMA QT (J105)

Connector: JST-SH 4-pin, 1mm pitch (`SM04B-SRSS-TB`)

| Pin | Signal | Voltage |
|-----|--------|---------|
| 1 | GND | — |
| 2 | +3.3V | 3.3V |
| 3 | SDA | 3.3V |
| 4 | SCL | 3.3V |

## OLED I2C Header (J111)

Connector: 1×4 pin socket, 2.54mm pitch

| Pin | Signal | Voltage |
|-----|--------|---------|
| 1 | GND | — |
| 2 | +3.3V | 3.3V |
| 3 | SCL | 3.3V |
| 4 | SDA | 3.3V |

## Dual I2C Header (J106)

Connector: 2×4 pin header, 2.54mm pitch (two identical I2C ports side by side)

| Left Pin | Signal | Right Pin | Signal |
|----------|--------|-----------|--------|
| 1 | +3.3V | 2 | +3.3V |
| 3 | SDA | 4 | SDA |
| 5 | GND | 6 | GND |
| 7 | SCL | 8 | SCL |

## I2C Solder Jumper Pads (JP101, JP102)

Two solder-bridge jumpers labelled "i2c" on the board. Per the schematic:

- **JP101**: Connects +3.3V pull-up to SDA (bridged by default)
- **JP102**: Connects +3.3V pull-up to SCL (bridged by default)

Cut the bridges to remove the on-board pull-ups if excessive pull-up current is an issue (e.g., many I2C devices on the bus).
