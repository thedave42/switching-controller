# VL53L0X Time-of-Flight Sensor Integration Plan

## Problem Statement

Add 5 VL53L0X Time-of-Flight distance sensors to the DCC-EX EX-CSB1 command
station for track block detection on the model railroad layout. The sensors
connect via the existing TCA9548A I2C multiplexer — no modifications to the
switching-controller firmware or Arduino Mega wiring are required. The design
should accommodate adding more sensors in the future.

## Approach

Two wiring approaches are documented below. Both are fully supported by
the CommandStation-EX VL53L0X HAL driver and require changes only in the
EX-CSB1's `myHal.cpp`.

| | Approach A — One Sensor per Channel | Approach B — Multiple Sensors per Channel |
|---|---|---|
| **Concept** | Each sensor on its own TCA9548A sub-bus | Multiple sensors share one sub-bus; XSHUT pins assign unique addresses at boot |
| **Wiring** | 4 wires per sensor (VCC, GND, SDA, SCL) | 4 shared I2C wires + 1 XSHUT wire per sensor to an EX-CSB1 GPIO pin |
| **Max sensors (1 mux)** | 7 (SubBus_1–7) | ~100 per channel (limited by I2C address space) |
| **Extra hardware** | None | None (uses EX-CSB1 GPIO pins). Optional: MCP23017 if GPIO pins are scarce |
| **Complexity** | Simplest | Moderate — XSHUT wiring and address assignment |
| **Best when** | ≤ 7 sensors, simple layout | Many sensors, or mux channels needed for other devices |

Choose **Approach A** for 5 sensors — it's the simplest and leaves 2 mux
channels free. Switch to **Approach B** if you later need more than 7
sensors or want to reserve mux channels for other I2C device types.

---

## 1. System Topology

### Current I2C Topology

```
                                   TCA9548A (0x70)
                                   ┌───────────────────┐
    EX-CSB1 ── QWiic (3.3V) ──────►│ Upstream          │
   (I2C master)                    │                   │
   OLED (0x3C) on upstream bus     │ CH0 (SubBus_0) ───┼──► Level Shifter ──► Arduino Mega (0x65)
                                   │ CH1 (SubBus_1)    │    (Switching Controller)
                                   │ CH2 (SubBus_2)    │
                                   │ CH3 (SubBus_3)    │
                                   │ CH4 (SubBus_4)    │
                                   │ CH5 (SubBus_5)    │
                                   │ CH6 (SubBus_6)    │
                                   │ CH7 (SubBus_7)    │
                                   └───────────────────┘
```

### Approach A Topology — One Sensor per Channel

```
                                   TCA9548A (0x70)
                                   ┌───────────────────┐
    EX-CSB1 ── QWiic (3.3V) ──────►│ Upstream          │
   (I2C master)                    │                   │
   OLED (0x3C) on upstream bus     │ CH0 (SubBus_0) ───┼──► Level Shifter ──► Arduino Mega (0x65)
                                   │ CH1 (SubBus_1) ───┼──► VL53L0X Sensor 1 (0x29)
                                   │ CH2 (SubBus_2) ───┼──► VL53L0X Sensor 2 (0x29)
                                   │ CH3 (SubBus_3) ───┼──► VL53L0X Sensor 3 (0x29)
                                   │ CH4 (SubBus_4) ───┼──► VL53L0X Sensor 4 (0x29)
                                   │ CH5 (SubBus_5) ───┼──► VL53L0X Sensor 5 (0x29)
                                   │ CH6 (SubBus_6)    │    (available)
                                   │ CH7 (SubBus_7)    │    (available)
                                   └───────────────────┘
```

Each sensor keeps the default address (0x29) because the mux isolates
them on separate buses. Channels 6–7 remain available.

### Approach B Topology — Multiple Sensors per Channel

```
                                   TCA9548A (0x70)
                                   ┌───────────────────┐
    EX-CSB1 ── QWiic (3.3V) ──────►│ Upstream          │
   (I2C master)                    │                   │
   OLED (0x3C) on upstream bus     │ CH0 (SubBus_0) ───┼──► Level Shifter ──► Arduino Mega (0x65)
                                   │ CH1 (SubBus_1) ───┼──► VL53L0X Sensor 1 (→ 0x30)
                                   │                   │  ► VL53L0X Sensor 2 (→ 0x31)
                                   │                   │  ► VL53L0X Sensor 3 (→ 0x32)
                                   │                   │  ► VL53L0X Sensor 4 (→ 0x33)
                                   │                   │  ► VL53L0X Sensor 5 (→ 0x34)
                                   │ CH2 (SubBus_2)    │    (available)
                                   │ CH3–CH7           │    (available)
                                   └───────────────────┘

  EX-CSB1 GPIO pins ──── XSHUT wires ──── each sensor's XSHUT pin
```

All 5 sensors share SubBus_1. Each sensor's XSHUT pin is wired to a
digital output VPIN — either a native EX-CSB1 GPIO pin or an I/O
expander pin (see `IO_VL53L0X.h` line 75: "The digital output may be
an Arduino pin or an I/O extender pin"). At boot, the driver resets all
sensors via XSHUT, then brings each online individually and assigns it a
unique I2C address (0x30–0x34). Channels 2–7 remain available.

---

## 2. Approach A — One Sensor per Channel

### 2A.1. Hardware Wiring

Each GY-VL53L0XV2 breakout connects to one TCA9548A downstream channel
connector (J2–J6 for CH1–CH5). Only 4 wires are needed per sensor:

```
  TCA9548A Connector (Jx)         GY-VL53L0XV2 Breakout (J1)
  ┌────────────┐                  ┌────────────────────────┐
  │ Pin 1  GND ├──────────────────┤ Pin 2  GND             │
  │ Pin 2  VCC ├──────────────────┤ Pin 1  VIN  (3.3V)     │
  │ Pin 3  SCL ├──────────────────┤ Pin 3  SCL             │
  │ Pin 4  SDA ├──────────────────┤ Pin 4  SDA             │
  └────────────┘                  │ Pin 5  XSHUT  (NC)     │
                                  │ Pin 6  GPIO1  (NC)     │
                                  └────────────────────────┘
```

XSHUT and GPIO1 are left unconnected. The GY-VL53L0XV2 breakout has an
on-board 47kΩ pull-up on XSHUT (to internal 2.8V), so the sensor
operates normally without external XSHUT wiring.

#### Channel-to-Connector Mapping

| Sensor | SubBus   | TCA9548A Connector | Purpose                |
|--------|----------|--------------------|------------------------|
| —      | SubBus_0 | J1 (CH0)           | Switching Controller   |
| 1      | SubBus_1 | J2 (CH1)           | VL53L0X — Block A      |
| 2      | SubBus_2 | J3 (CH2)           | VL53L0X — Block B      |
| 3      | SubBus_3 | J4 (CH3)           | VL53L0X — Block C      |
| 4      | SubBus_4 | J5 (CH4)           | VL53L0X — Block D      |
| 5      | SubBus_5 | J6 (CH5)           | VL53L0X — Block E      |
| —      | SubBus_6 | J7 (CH6)           | (available)            |
| —      | SubBus_7 | J8 (CH7)           | (available)            |

> **Note:** "Block A–E" are placeholder labels. Replace with your actual
> track block names when wiring.

### 2A.2. I2C Pull-Ups

No external pull-up resistors are needed. Each GY-VL53L0XV2 breakout
includes 10kΩ pull-ups on SCL and SDA (to VIN). Since each sensor is
the only device on its sub-bus, these pull-ups are sufficient.

The TCA9548A breakout does **not** provide pull-ups on the downstream
channels — the sensor breakout supplies them.

### 2A.3. Power

All 5 sensors are powered from the TCA9548A's VCC rail (3.3V from the
EX-CSB1). The GY-VL53L0XV2 breakout accepts 3.3–5V on VIN and has an
on-board 2.8V LDO for the VL53L0X chip.

| Parameter              | Per Sensor | 5 Sensors |
|------------------------|------------|-----------|
| Active current (typ.)  | ~19 mA     | ~95 mA    |
| Standby current        | ~5 µA      | ~25 µA    |

Verify that the EX-CSB1's 3.3V regulator can supply the total load
(switching controller level shifter + 5 sensors + any other 3.3V
peripherals). Most ESP32 boards provide 500 mA–1 A on the 3.3V rail,
which is well above the ~95 mA sensor draw.

If the 3.3V rail is insufficient, the sensors can be powered from a
separate 3.3V or 5V supply with a common ground to the EX-CSB1.

---

### 2A.4. Software Configuration

#### Overview

All changes are in the EX-CSB1's `CommandStation-EX/myHal.cpp`. No
changes to the switching-controller firmware or the Arduino Mega are
needed.

#### Required Include

Add the VL53L0X header at the top of `myHal.cpp`:

```cpp
#include "IO_VL53L0X.h"
```

#### VL53L0X Device Registration

Add one `VL53L0X::create()` call per sensor in `halSetup()`:

```cpp
VL53L0X::create(firstVpin, nPins, i2cAddress, lowThreshold, highThreshold);
```

| Parameter        | Description                                                    |
|------------------|----------------------------------------------------------------|
| `firstVpin`      | First VPIN allocated to this sensor                            |
| `nPins`          | Number of VPINs: 1 (digital only), 2 (+distance), or 3 (+signal/ambient) |
| `i2cAddress`     | `{SubBus_X, 0x29}` — mux channel + default address            |
| `lowThreshold`   | Distance (mm) at which the digital VPIN goes HIGH (object detected) |
| `highThreshold`  | Distance (mm) at which the digital VPIN goes LOW (object gone) |

The low/high thresholds provide hysteresis to prevent rapid toggling
when an object is near the boundary.

#### VPIN Allocation (3 VPINs per sensor)

Using `nPins = 3` gives full access to each sensor's readings:

| Sensor | SubBus   | VPINs       | VPIN +0 (digital) | VPIN +1 (distance mm) | VPIN +2 (signal strength) |
|--------|----------|-------------|--------------------|-----------------------|---------------------------|
| 1      | SubBus_1 | 5000–5002   | 5000               | 5001                  | 5002                      |
| 2      | SubBus_2 | 5003–5005   | 5003               | 5004                  | 5005                      |
| 3      | SubBus_3 | 5006–5008   | 5006               | 5007                  | 5008                      |
| 4      | SubBus_4 | 5009–5011   | 5009               | 5010                  | 5011                      |
| 5      | SubBus_5 | 5012–5014   | 5012               | 5013                  | 5014                      |

- **Digital VPIN (firstVpin + 0):** Returns 1 when measured distance < `lowThreshold`, returns 0 when distance > `highThreshold`.
- **Distance VPIN (firstVpin + 1):** Returns the measured distance in millimeters (analogue read). Max range ~1200 mm.
- **Signal VPIN (firstVpin + 2):** Returns the return signal strength (analogue read). Useful for diagnosing reflectivity issues.

#### Sensor Objects for DCC-EX Protocol

To receive `<Q sensorId>` / `<q sensorId>` messages when sensors trigger
(for JMRI, EX-RAIL automation, or serial monitoring), create a `Sensor`
object for each digital VPIN:

```cpp
Sensor::create(sensorId, vpin, 0);
```

#### Complete myHal.cpp (Approach A)

```cpp
// myHal.cpp — DCC-EX HAL configuration for Switching Controller
//
// Copy this file and IO_SwitchingController.h to your EX-CommandStation
// source folder. See the Arduino IDE setup instructions at:
// https://dcc-ex.com/reference/developers/hal-config.html#adding-a-new-device

#include "IODevice.h"
#include "IO_SwitchingController.h"
#include "IO_VL53L0X.h"

void halSetup() {
  // ── Switching Controller ──────────────────────────────────────────────
  // 12 turnout VPINs (800–811) on TCA9548A CH0 at address 0x65
  SwitchingController::create(800, 12, I2CAddress(I2CMux_0, SubBus_0, 0x65));

  // ── VL53L0X Time-of-Flight Sensors ───────────────────────────────────
  // 3 VPINs per sensor: digital (threshold), distance (mm), signal strength
  // Thresholds: object detected at <200mm, cleared at >250mm (tune per site)
  //
  //                   vpin  nPins  {SubBus, addr}      low  high
  VL53L0X::create(    5000, 3,     {SubBus_1, 0x29},   200, 250);  // Sensor 1
  VL53L0X::create(    5003, 3,     {SubBus_2, 0x29},   200, 250);  // Sensor 2
  VL53L0X::create(    5006, 3,     {SubBus_3, 0x29},   200, 250);  // Sensor 3
  VL53L0X::create(    5009, 3,     {SubBus_4, 0x29},   200, 250);  // Sensor 4
  VL53L0X::create(    5012, 3,     {SubBus_5, 0x29},   200, 250);  // Sensor 5

  // ── DCC-EX Sensor Objects ────────────────────────────────────────────
  // Report sensor state changes via <Q id> / <q id> messages
  Sensor::create(5000, 5000, 0);   // Sensor 1
  Sensor::create(5003, 5003, 0);   // Sensor 2
  Sensor::create(5006, 5006, 0);   // Sensor 3
  Sensor::create(5009, 5009, 0);   // Sensor 4
  Sensor::create(5012, 5012, 0);   // Sensor 5
}
```

#### Threshold Tuning

The `lowThreshold` and `highThreshold` values (200 mm / 250 mm above) are
starting points. Tune per sensor based on mounting distance and rolling stock:

- **lowThreshold:** Distance (mm) at which the sensor reports "occupied." Set
  this slightly above the expected distance to the top of the train when
  passing under the sensor.
- **highThreshold:** Distance (mm) at which the sensor reports "clear." Set
  this 20–50 mm above `lowThreshold` to provide hysteresis and prevent
  flicker.
- The VL53L0X maximum reliable range is ~1200 mm. Beyond that, readings
  become noisy.

Use the DCC-EX serial command `<JA vpin>` to read the live distance value
from any sensor's distance VPIN to help dial in thresholds:

```
<JA 5001>    → returns distance in mm for Sensor 1
<JA 5004>    → returns distance in mm for Sensor 2
```

---

## 3. Approach B — Multiple Sensors per Channel (XSHUT)

### 3B.1. How It Works

All VL53L0X sensors power on at the default I2C address (0x29). To run
multiple sensors on the same I2C bus, the CommandStation-EX VL53L0X
driver uses the XSHUT (shutdown) pin on each module to sequence address
assignment at boot. From `IO_VL53L0X.h`:

> When the device driver starts, the XSHUT pin is set LOW to turn the
> module off. Once all VL53L0X modules are turned off, the driver works
> through each module in turn, setting XSHUT to HIGH to turn that module
> on, then writing that module's desired I2C address.

The sequence is:

1. Driver pulls **all** XSHUT pins LOW → all sensors enter hardware
   standby and release the bus.
2. Driver releases XSHUT on **Sensor 1** (sets pin to high-impedance;
   on-board 47kΩ pull-up wakes the sensor at address 0x29).
3. Driver writes the desired unique address (e.g., 0x30) to Sensor 1.
4. Sensor 1 now responds only at 0x30. Driver moves to Sensor 2.
5. Repeat for each sensor until all have unique addresses.

### 3B.2. XSHUT Pin Control

Each sensor's XSHUT pin must be wired to a digital output VPIN. From
`IO_VL53L0X.h` line 75:

> xshutPin is the VPIN number corresponding to a digital output that is
> connected to the XSHUT terminal on the module. **The digital output may
> be an Arduino pin or an I/O extender pin.**

Two options for providing these VPINs:

**Option 1 — Native EX-CSB1 GPIO pins (simplest, no extra hardware).**
Wire each sensor's XSHUT directly to a free GPIO pin on the EX-CSB1.
Native pins are VPINs directly (pin N = VPIN N). This is the approach
used in `myHal.cpp.txt` line 363:

```cpp
VL53L0X::create(5000, 3, {SubBus_0, 0x60}, 300, 310, 46);
//                                               xshut pin ^
```

**Option 2 — I/O expander (when native GPIO pins are limited).**
Add an MCP23017 (or similar) on the same or another mux channel to
provide additional digital output VPINs. This is the approach shown in
`myHal.cpp_example.txt` lines 200–205:

```cpp
// XSHUT pins connected to first two pins of MCP23017 (164 and 165)
VL53L0X::create(5000, 1, 0x30, 200, 250, 164);
VL53L0X::create(5001, 1, 0x31, 200, 250, 165);
```

### 3B.3. Hardware Wiring

#### Shared I2C Bus

All sensors on SubBus_1 share the same SDA/SCL lines from the TCA9548A
J2 connector:

```
  TCA9548A J2 (CH1)
  ┌────────────┐
  │ Pin 1  GND ├──────┬──────┬──────┬──────┬────── GND
  │ Pin 2  VCC ├──────┼──────┼──────┼──────┼────── 3.3V
  │ Pin 3  SCL ├──────┼──────┼──────┼──────┼────── SCL bus
  │ Pin 4  SDA ├──────┼──────┼──────┼──────┼────── SDA bus
  └────────────┘      │      │      │      │
                      S1     S2     S3     S4     S5
                    (→0x30)(→0x31)(→0x32)(→0x33)(→0x34)
```

All GND, VCC, SCL, and SDA pins are bused together. Use a small
breadboard or solder bus board to split the 4 I2C lines out to each
sensor.

#### XSHUT Wiring

Each sensor's XSHUT pin (GY-VL53L0XV2 J1 Pin 5) connects to a digital
output VPIN via a single wire:

```
  Digital Output VPINs          GY-VL53L0XV2 Breakouts
  (native GPIO or expander)
  ─────────────────────
  VPIN A ──────────────────────── Sensor 1  Pin 5 (XSHUT)
  VPIN B ──────────────────────── Sensor 2  Pin 5 (XSHUT)
  VPIN C ──────────────────────── Sensor 3  Pin 5 (XSHUT)
  VPIN D ──────────────────────── Sensor 4  Pin 5 (XSHUT)
  VPIN E ──────────────────────── Sensor 5  Pin 5 (XSHUT)
```

> **Note on XSHUT voltage:** The VL53L0X driver sets the XSHUT control
> pin to high-impedance (input mode) to release the sensor from shutdown.
> The on-board 47kΩ pull-up on the GY-VL53L0XV2 breakout brings XSHUT
> HIGH to the internal 2.8V rail. This means the driving pin does not
> force a voltage onto XSHUT — it only needs to sink to GND (to hold
> shutdown) or float (to release). See `IO_VL53L0X.h` lines 178–195.

#### Per-Sensor I2C + XSHUT Connection

```
  Shared I2C Bus                  GY-VL53L0XV2 Breakout (J1)
  ─────────────                   ┌────────────────────────┐
   GND  ──────────────────────────┤ Pin 2  GND             │
   VCC (3.3V)  ───────────────────┤ Pin 1  VIN             │
   SCL  ──────────────────────────┤ Pin 3  SCL             │
   SDA  ──────────────────────────┤ Pin 4  SDA             │
   XSHUT VPIN ────────────────────┤ Pin 5  XSHUT           │
                                  │ Pin 6  GPIO1  (NC)     │
                                  └────────────────────────┘
```

### 3B.4. I2C Pull-Ups (Shared Bus)

When multiple sensor breakouts share one sub-bus, their 10kΩ pull-ups
are in parallel. Five sensors yield an effective pull-up of
10kΩ / 5 = 2kΩ.

2kΩ is within the acceptable I2C pull-up range for 3.3V at 100 kHz.
If you add significantly more sensors (>8), measure the bus with an
oscilloscope to verify clean signal edges, and remove some pull-up
resistors from the sensor breakouts if needed (desolder R1/R2 on extra
GY-VL53L0XV2 boards).

### 3B.5. Address Assignment Table

| Sensor | Assigned Address | XSHUT VPIN |
|--------|-----------------|------------|
| 1      | 0x30            | (your pin) |
| 2      | 0x31            | (your pin) |
| 3      | 0x32            | (your pin) |
| 4      | 0x33            | (your pin) |
| 5      | 0x34            | (your pin) |

Addresses 0x30–0x34 are chosen to avoid conflicts with common I2C
devices. Any valid 7-bit address works **except** 0x29 (the default).
If using an MCP23017 on the same bus, also avoid its address (0x20).
Avoid 0x50 (FRAM) and 0x70–0x77 (TCA9548A range).

### 3B.6. Software Configuration

#### myHal.cpp (Approach B — using native GPIO pins)

This example uses EX-CSB1 native GPIO pins for XSHUT, matching the
pattern from `myHal.cpp.txt` line 363. Replace the pin numbers with
your actual free GPIO pins.

```cpp
// myHal.cpp — DCC-EX HAL configuration for Switching Controller
//
// Copy this file and IO_SwitchingController.h to your EX-CommandStation
// source folder. See the Arduino IDE setup instructions at:
// https://dcc-ex.com/reference/developers/hal-config.html#adding-a-new-device

#include "IODevice.h"
#include "IO_SwitchingController.h"
#include "IO_VL53L0X.h"

void halSetup() {
  // ── Switching Controller ──────────────────────────────────────────────
  // 12 turnout VPINs (800–811) on TCA9548A CH0 at address 0x65
  SwitchingController::create(800, 12, I2CAddress(I2CMux_0, SubBus_0, 0x65));

  // ── VL53L0X Time-of-Flight Sensors ───────────────────────────────────
  // All sensors on SubBus_1, each with a unique address and XSHUT VPIN.
  // XSHUT VPINs are native EX-CSB1 GPIO pins (replace with your free pins).
  // 3 VPINs per sensor: digital (threshold), distance (mm), signal strength
  // Thresholds: object detected at <200mm, cleared at >250mm (tune per site)
  //
  //                   vpin  nPins  {SubBus, addr}      low  high  xshut
  VL53L0X::create(    5000, 3,     {SubBus_1, 0x30},   200, 250,  46);   // Sensor 1
  VL53L0X::create(    5003, 3,     {SubBus_1, 0x31},   200, 250,  47);   // Sensor 2
  VL53L0X::create(    5006, 3,     {SubBus_1, 0x32},   200, 250,  48);   // Sensor 3
  VL53L0X::create(    5009, 3,     {SubBus_1, 0x33},   200, 250,  49);   // Sensor 4
  VL53L0X::create(    5012, 3,     {SubBus_1, 0x34},   200, 250,  50);   // Sensor 5

  // ── DCC-EX Sensor Objects ────────────────────────────────────────────
  // Report sensor state changes via <Q id> / <q id> messages
  Sensor::create(5000, 5000, 0);   // Sensor 1
  Sensor::create(5003, 5003, 0);   // Sensor 2
  Sensor::create(5006, 5006, 0);   // Sensor 3
  Sensor::create(5009, 5009, 0);   // Sensor 4
  Sensor::create(5012, 5012, 0);   // Sensor 5
}
```

#### myHal.cpp (Approach B — using MCP23017 I/O expander)

If native GPIO pins are not available, add an MCP23017 on the same mux
channel. This matches the pattern from `myHal.cpp_example.txt` lines
200–205. The `MCP23017::create()` call **must** appear before the
`VL53L0X::create()` calls so it is initialized first.

```cpp
#include "IODevice.h"
#include "IO_SwitchingController.h"
#include "IO_VL53L0X.h"
#include "IO_MCP23017.h"

void halSetup() {
  SwitchingController::create(800, 12, I2CAddress(I2CMux_0, SubBus_0, 0x65));

  // MCP23017: 16 GPIO VPINs (164–179) on SubBus_1, address 0x20
  MCP23017::create(164, 16, {SubBus_1, 0x20});

  //                   vpin  nPins  {SubBus, addr}      low  high  xshut
  VL53L0X::create(    5000, 3,     {SubBus_1, 0x30},   200, 250,  164);  // Sensor 1
  VL53L0X::create(    5003, 3,     {SubBus_1, 0x31},   200, 250,  165);  // Sensor 2
  VL53L0X::create(    5006, 3,     {SubBus_1, 0x32},   200, 250,  166);  // Sensor 3
  VL53L0X::create(    5009, 3,     {SubBus_1, 0x33},   200, 250,  167);  // Sensor 4
  VL53L0X::create(    5012, 3,     {SubBus_1, 0x34},   200, 250,  168);  // Sensor 5

  Sensor::create(5000, 5000, 0);
  Sensor::create(5003, 5003, 0);
  Sensor::create(5006, 5006, 0);
  Sensor::create(5009, 5009, 0);
  Sensor::create(5012, 5012, 0);
}
```

#### VPIN Allocation (Approach B)

| Device     | VPINs     | SubBus   | Address |
|------------|-----------|----------|---------|
| Sensor 1   | 5000–5002 | SubBus_1 | 0x30    |
| Sensor 2   | 5003–5005 | SubBus_1 | 0x31    |
| Sensor 3   | 5006–5008 | SubBus_1 | 0x32    |
| Sensor 4   | 5009–5011 | SubBus_1 | 0x33    |
| Sensor 5   | 5012–5014 | SubBus_1 | 0x34    |

(If using MCP23017, it also occupies VPINs 164–179.)

#### Adding More Sensors (Approach B)

Continue the pattern with unique addresses and additional XSHUT VPINs:

```cpp
VL53L0X::create(5015, 3, {SubBus_1, 0x35}, 200, 250, xshutVpin6);  // Sensor 6
VL53L0X::create(5018, 3, {SubBus_1, 0x36}, 200, 250, xshutVpin7);  // Sensor 7
```

---

## 4. Verification and Testing

### Step 1 — I2C Bus Scan

After wiring each sensor, power up the EX-CSB1 and check the serial
console for I2C device detection. DCC-EX scans the I2C bus at startup
and logs discovered devices including muxed sub-buses.

**Approach A** — look for each sensor on its own sub-bus:
```
I2C Device found at {I2CMux_0,SubBus_1,0x29}
I2C Device found at {I2CMux_0,SubBus_2,0x29}
...
```

**Approach B** — look for sensors on a shared sub-bus (after address
assignment; at initial scan they will all be at 0x29):
```
I2C Device found at {I2CMux_0,SubBus_1,0x30}
I2C Device found at {I2CMux_0,SubBus_1,0x31}
...
```

### Step 2 — HAL Initialization

With `myHal.cpp` updated, the startup log should show VL53L0X devices
initializing:

**Approach A:**
```
VL53L0X I2C:{I2CMux_0,SubBus_1,0x29} Configured on Vpins:5000-5002
VL53L0X I2C:{I2CMux_0,SubBus_2,0x29} Configured on Vpins:5003-5005
...
```

**Approach B:**
```
VL53L0X I2C:{I2CMux_0,SubBus_1,0x30} Configured on Vpins:5000-5002
VL53L0X I2C:{I2CMux_0,SubBus_1,0x31} Configured on Vpins:5003-5005
...
```

If a sensor fails to initialize, you'll see:
```
VL53L0X I2C:{I2CMux_0,SubBus_X,0xYY} device not responding
```

### Step 3 — Live Distance Readings

Use the `<JA vpin>` serial command to read analogue values:

```
<JA 5001>    → Sensor 1 distance (mm)
<JA 5004>    → Sensor 2 distance (mm)
<JA 5007>    → Sensor 3 distance (mm)
<JA 5010>    → Sensor 4 distance (mm)
<JA 5013>    → Sensor 5 distance (mm)
```

Wave your hand in front of each sensor and confirm the distance changes
appropriately.

### Step 4 — Digital Threshold Testing

Use `<JR vpin>` to read the digital state, or watch for `<Q id>` / `<q id>`
messages when objects enter/leave the detection zone:

```
<JR 5000>    → 0 (no object) or 1 (object detected) for Sensor 1
```

Place an object within the threshold range and confirm `<Q 5000>` is sent.
Remove it and confirm `<q 5000>` is sent.

### Step 5 — Signal Strength Check

Read the signal strength VPIN to verify good reflectivity:

```
<JA 5002>    → Sensor 1 signal strength
```

Low signal strength may indicate the sensor is aimed at a non-reflective
surface, is too far from the target, or ambient light is interfering.

---

## 5. EX-RAIL Automation Examples

Once sensors are active, they can drive layout automation via EX-RAIL
scripts in `myAutomation.h`:

```cpp
// Block detection: stop train when sensor 1 detects occupancy
ROUTE(1, "Approach Block A")
  AT(5000)                  // Wait for sensor 1 to trigger
  STOP                      // Stop the current train
DONE

// Automatic crossing gate
AUTOMATION(2, "Crossing Gate")
  AT(5000)                  // Train approaching
  CLOSE(100)                // Lower the gate (servo on VPIN 100)
  AFTER(5000)               // Wait for sensor to clear
  DELAY(3000)               // Hold gate 3 more seconds
  THROW(100)                // Raise the gate
DONE
```

---

## 6. Future Expansion

### Approach A — Adding Sensors 6 and 7

Two mux channels remain available (SubBus_6 and SubBus_7). To add sensors:

```cpp
VL53L0X::create(5015, 3, {SubBus_6, 0x29}, 200, 250);  // Sensor 6
VL53L0X::create(5018, 3, {SubBus_7, 0x29}, 200, 250);  // Sensor 7
Sensor::create(5015, 5015, 0);
Sensor::create(5018, 5018, 0);
```

This brings the maximum to **7 VL53L0X sensors** (SubBus_1 through
SubBus_7) using the one-sensor-per-channel approach. To go beyond 7,
switch to Approach B or add a second TCA9548A.

### Approach B — Scaling Beyond 5 Sensors

With Approach B, adding more sensors requires only a new
`VL53L0X::create()` line per sensor plus an additional XSHUT VPIN
(see §3B.6). The limit per channel depends on available XSHUT VPINs
and I2C address space.

Spread sensor groups across multiple sub-bus channels if a single
channel becomes congested (too many parallel pull-ups or too many
addresses).

### Adding a Second TCA9548A Multiplexer

A second TCA9548A at address 0x71 (A0=HIGH, A1=GND, A2=GND) provides
8 more channels. Either approach works on the second mux:

```cpp
// Approach A on second mux
VL53L0X::create(5021, 3, {I2CMux_1, SubBus_0, 0x29}, 200, 250);

// Approach B on second mux
VL53L0X::create(5021, 3, {I2CMux_1, SubBus_0, 0x30}, 200, 250, xshutVpin);
```

Wire the second TCA9548A's upstream SDA/SCL to the EX-CSB1's I2C bus
(same as the first mux).

---

## 7. Considerations and Constraints

### Sensor Mounting

- The VL53L0X emits a 940 nm (infrared) laser in a 25° cone. Mount the
  sensor so the cone covers the track area where detection is needed.
- Keep the sensor's cover glass clean and free of obstructions within
  ~10 mm.
- Avoid pointing sensors at highly reflective surfaces (mirrors, polished
  metal) which can cause false short-distance readings.
- Dark or matte surfaces may reduce range. Use the signal strength VPIN
  to verify adequate reflectivity.

### Cable Length

- I2C is reliable up to ~1 meter with standard pull-ups.
- The TCA9548A isolates each channel, so bus capacitance on one channel
  doesn't affect others.
- For runs longer than 1 meter, consider using shielded cable (ground the
  shield at one end only) or reducing I2C clock speed.
- The GY-VL53L0XV2 breakout's 10kΩ pull-ups are adequate for short runs.
  For longer runs, stronger pull-ups (e.g., 4.7kΩ) may be needed on the
  channel — solder them between SCL/VCC and SDA/VCC at the sensor end.

### Scan Rate

The VL53L0X takes ~60 ms per measurement. The DCC-EX HAL driver polls
on a 100 ms cycle (10 readings/second per sensor). With 5 sensors, the
mux switches channels sequentially, but since each sensor runs its own
measurement cycle asynchronously, the effective update rate remains
~10 Hz per sensor. This is more than sufficient for model railroad
speeds.

### Ambient Light

Strong ambient infrared light (direct sunlight, halogen lamps) can
reduce the sensor's effective range and increase noise. Indoor layout
lighting with LED or fluorescent bulbs is generally fine.

---

## 8. Implementation Checklist

### Approach A

1. **Wire sensors** — Connect each GY-VL53L0XV2 breakout to TCA9548A
   channels J2–J6 (CH1–CH5) using the 4-wire diagram in §2A.1.
2. **Verify I2C detection** — Power up and check the EX-CSB1 serial log
   for device detection on SubBus_1 through SubBus_5.
3. **Update myHal.cpp** — Add `#include "IO_VL53L0X.h"` and the
   `VL53L0X::create()` / `Sensor::create()` calls per §2A.4.
4. **Flash EX-CSB1** — Compile and upload the updated CommandStation-EX
   firmware.
5. **Verify initialization** — Check serial log for successful VL53L0X
   configuration messages.
6. **Test live readings** — Use `<JA vpin>` commands to read distances.
7. **Tune thresholds** — Adjust per sensor based on mounting geometry.
8. **Add EX-RAIL automation** — Write scripts in `myAutomation.h`.

### Approach B

1. **Wire sensor I2C** — Connect all 5 GY-VL53L0XV2 breakouts to the
   same TCA9548A channel (shared SDA/SCL/VCC/GND bus) per §3B.3.
2. **Wire XSHUT** — Run a wire from each sensor's XSHUT pin (J1 Pin 5)
   to a digital output VPIN — either a free EX-CSB1 GPIO pin or an I/O
   expander pin (see §3B.2 for both options).
3. **Verify I2C detection** — Power up and check the serial log for
   sensor detection on the shared sub-bus.
4. **Update myHal.cpp** — Add includes and create calls per §3B.6.
   If using an I/O expander, create it **before** VL53L0X devices.
5. **Flash EX-CSB1** — Compile and upload.
6. **Verify initialization** — Check serial log for address assignment
   and successful configuration of all 5 sensors.
7. **Test live readings** — Use `<JA vpin>` commands to read distances.
8. **Tune thresholds** — Adjust per sensor based on mounting geometry.
9. **Add EX-RAIL automation** — Write scripts in `myAutomation.h`.
