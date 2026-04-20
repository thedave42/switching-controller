# GY-VL53L0XV2 Time-of-Flight Distance Sensor Breakout — Circuit Diagram

Reference schematic for the GY-VL53L0XV2 breakout board (ST VL53L0X
Time-of-Flight ranging sensor with on-board 2.8V LDO and bidirectional
I²C level shifters). This board accepts a 3.3–5V supply and is safe to
use directly on a 5V Arduino Mega I²C bus.

```
╔══════════════════════════════════════════════════════════════════════════════════╗
║              GY-VL53L0XV2 TIME-OF-FLIGHT DISTANCE SENSOR BREAKOUT              ║
╚══════════════════════════════════════════════════════════════════════════════════╝


═══════════════════════════════════════════════════════════════════════════════════
 1. MAIN IC — VL53L0X (U1)
═══════════════════════════════════════════════════════════════════════════════════

  ST Microelectronics VL53L0X — 940 nm laser-based ToF ranging sensor,
  I²C interface, default address 0x29, OPLGA-12 package. The internal
  I/O domain runs at 2.8V (AVDD), supplied by the on-board LDO.

                            ┌───────────────────────┐
                  SCL ──10  │ SCL          AVDD    │ 11 ── VDD (2.8V)
                  SDA ── 9  │ SDA   VL53L0X        │
                            │       (U1)   AVDDVCSEL│ ── VDD
                XSHUT ── 5  │ XSHUT          DNC   │ 8
                GPIO1 ── 7  │ GPIO1          GND2  │ 6 ── GND
                            │                GND3  │ 4 ── GND
                            │                GND4  │ 12 ── GND
                            │       AVSSVCSEL(GND) │ ── GND
                            └───────────────────────┘


═══════════════════════════════════════════════════════════════════════════════════
 2. I²C ADDRESS
═══════════════════════════════════════════════════════════════════════════════════

  Default 7-bit I²C address: 0x29 (fixed in silicon — there are no
  address-select pins). To run multiple VL53L0X sensors on the same
  bus, hold all but one in reset via XSHUT, then re-program the active
  device's address at runtime, then release the next sensor's XSHUT.


═══════════════════════════════════════════════════════════════════════════════════
 3. ON-BOARD LDO REGULATOR (2.8V)
═══════════════════════════════════════════════════════════════════════════════════

  A small SOT-23-5 LDO (silkscreen "663K", typically an XC6206-style
  2.8V regulator) drops the input rail (VIN, 3.3–5V) to the 2.8V
  supply rail (VDD) used by the VL53L0X and the level shifter
  reference.

         VIN ─────┬───────┐
                  │       │
             ┌────┴────┐  │ Pin 1 (IN)        Pin 5 (OUT)         VDD (2.8V)
             │  C1     │  │      ┌──────────┐                      │
             │  1µF    │  └──────┤ IN   OUT ├──────┬───────────────┤
             └────┬────┘         │          │      │               │
                  │              │   LDO    │      │           ┌───┴───┐
                 GND      ┌──────┤ ON/OFF NC│ Pin 4│           │  C3   │  1µF
                          │      │          │      │           │       │
                  Pin 3 ──┘      │   GND    │      │           └───┬───┘
                  (ON/OFF        └────┬─────┘      │               │
                   tied                Pin 2       │              GND
                   to VIN              GND         │
                   = always on)         │      ┌───┴───┐
                                       GND     │  C4   │  4.7µF
                                               │       │
                                               └───┬───┘
                                                   │
                                                  GND


═══════════════════════════════════════════════════════════════════════════════════
 4. BIDIRECTIONAL I²C LEVEL SHIFTER (SCL / SDA)
═══════════════════════════════════════════════════════════════════════════════════

  The breakout uses the classic Philips/NXP MOSFET level shifter
  (one N-channel MOSFET per line) to translate between the host's
  3.3V/5V I²C bus and the VL53L0X's 2.8V I/O domain.

  - SCL  (host side, pulled to VIN via R1) ─┐                ┌─ SCLLV (2.8V side, pulled to VDD via R3) ── U1 Pin 10 (SCL)
                                            │   Q1A NMOS     │
                                       gate ┼───── VDD ──────┤
                                       drain┴────────────────┘
                                       source ── (host side)

  - SDA  (host side, pulled to VIN via R2) ─┐                ┌─ SDALV (2.8V side, pulled to VDD via R4) ── U1 Pin 9 (SDA)
                                            │   Q1B NMOS     │
                                       gate ┼───── VDD ──────┤
                                       drain┴────────────────┘
                                       source ── (host side)

  Pull-up resistors:
    R1 (SCL host side)  10kΩ to VIN
    R2 (SDA host side)  10kΩ to VIN
    R3 (SCL 2.8V side)  10kΩ to VDD
    R4 (SDA 2.8V side)  10kΩ to VDD

  XSHUT and GPIO1 also pass through pull-ups on the 2.8V side:
    R5 (XSHUT) 47kΩ to VDD
    R6 (GPIO1) 47kΩ to VDD


═══════════════════════════════════════════════════════════════════════════════════
 5. XSHUT (Shutdown / Reset, Active LOW)
═══════════════════════════════════════════════════════════════════════════════════

  XSHUT is the VL53L0X hardware reset / shutdown line. It is held HIGH
  by R5 (47kΩ to VDD = 2.8V). Driving it LOW puts the sensor into
  hardware standby (the I²C interface is unresponsive). Releasing it
  causes a fresh boot.

  XSHUT is broken out to header pin 5 and is safe to drive directly
  from a 3.3V or 5V GPIO (the on-board pull-up is to 2.8V, so the line
  will be clamped near VDD by the GPIO when driven HIGH; for cleaner
  level translation, drive it LOW only and let R5 pull it HIGH, i.e.
  treat the GPIO as open-drain).

             VDD (2.8V)
                  │
              ┌───┴───┐
              │ R5    │  47kΩ pull-up
              │ 47kΩ  │
              └───┬───┘
                  │
                  ├──────── U1 Pin 5 (XSHUT)
                  │
                  └──────── Header Pin 5 (XSHUT)


═══════════════════════════════════════════════════════════════════════════════════
 6. GPIO1 (Interrupt Output)
═══════════════════════════════════════════════════════════════════════════════════

  GPIO1 is the VL53L0X's programmable interrupt output (e.g. "new
  measurement ready", or threshold-crossing). It is open-drain inside
  the chip and pulled HIGH by R6 (47kΩ to VDD = 2.8V). Because the
  high level is only 2.8V, an Arduino reading it as a digital input
  is fine (above the 5V/3.3V logic-HIGH threshold for most MCUs at
  2.8V — confirm against your board's V_IH spec).

             VDD (2.8V)
                  │
              ┌───┴───┐
              │ R6    │  47kΩ pull-up
              │ 47kΩ  │
              └───┬───┘
                  │
                  ├──────── U1 Pin 7 (GPIO1)
                  │
                  └──────── Header Pin 6 (GPIO1)


═══════════════════════════════════════════════════════════════════════════════════
 7. BREAKOUT HEADER — J1 (6-pin)
═══════════════════════════════════════════════════════════════════════════════════

  Single 6-pin 0.1" header along the long edge of the board. Pin order
  matches the silkscreen on the GY-VL53L0XV2 board.

  ┌────────────────────────────┐
  │  J1 — 6 Pin                │
  │                            │
  │  Pin 1 ── VIN              │  ── LDO input, 3.3–5V
  │  Pin 2 ── GND              │  ── common ground
  │  Pin 3 ── SCL              │  ── host-side SCL (level-shifted), R1 10kΩ pull-up to VIN
  │  Pin 4 ── SDA              │  ── host-side SDA (level-shifted), R2 10kΩ pull-up to VIN
  │  Pin 5 ── XSHUT            │  ── 2.8V logic, R5 47kΩ pull-up to VDD
  │  Pin 6 ── GPIO1            │  ── 2.8V logic, R6 47kΩ pull-up to VDD
  │                            │
  └────────────────────────────┘


═══════════════════════════════════════════════════════════════════════════════════
 FULL BOARD SCHEMATIC
═══════════════════════════════════════════════════════════════════════════════════

       VIN                                                       VDD (2.8V from LDO)
        │                                                              │
        ├─────┬──── R1 10kΩ ──┬─ SCL (host) ──┐                        │
        │     │                │                │  Q1A NMOS              │
        │     │                │           gate ┼───── VDD ──────────────┤
        │     │                │                │                        │
        │     │                │           drain┴── SCLLV ──── R3 10kΩ ──┤
        │     │                │                                         │
        │     ├──── R2 10kΩ ──┬─ SDA (host) ──┐                          │
        │     │                │                │  Q1B NMOS              │
        │     │                │           gate ┼───── VDD ──────────────┤
        │     │                │                │                        │
        │     │                │           drain┴── SDALV ──── R4 10kΩ ──┤
        │     │                │                                         │
        │     │                │                                         │
        │  ┌──┴──┐             │                                         │
        │  │ C1  │  1µF        │                                         │
        │  │     │             │                                         │
        │  └──┬──┘             │                                         │
        │     │                │                                         │
        │    GND               │                                         │
        │                      │                                         │
        │   ┌────────────────┐ │                                         │
        ├──→│ Pin 1 IN       │ │                                         │
        ├──→│ Pin 3 ON/OFF   │ │       SCLLV ────────────── U1 Pin 10 (SCL)
        │   │      LDO   OUT │→┴── VDD                                   │
        │   │      (663K)    │                                           │
        │   │      NC        │ Pin 4                                     │
        │   │      GND       │ Pin 2 ── GND                              │
        │   └────────────────┘                                           │
        │                                                                │
        │                              SDALV ─────────────── U1 Pin 9  (SDA)
        │                                                                │
        │                                                                │
        │                       ┌── R5 47kΩ ──┬── U1 Pin 5  (XSHUT) ─────┤
        │                       │              └── J1 Pin 5 (XSHUT)      │
        │                       │                                        │
        │                       ├── R6 47kΩ ──┬── U1 Pin 7  (GPIO1) ─────┤
        │                       │              └── J1 Pin 6 (GPIO1)      │
        │                       │                                        │
        │                  ┌────┴────┐                                   │
        │                  │   C3    │  1µF                              │
        │                  │         │                                   │
        │                  └────┬────┘                                   │
        │                       │                                        │
        │                      GND                                       │
        │                                                                │
        │                                              ┌────┐            │
        │                                              │ C4 │  4.7µF     │
        │                                              │    │            │
        │                                              └─┬──┘            │
        │                                                │               │
        │                                               GND              │
        │                                                                │
        │                                       U1 Pin 11 (AVDD) ────────┤
        │                                       U1 AVDDVCSEL    ─────────┘
        │
        │                                       U1 Pins 4, 6, 12, AVSSVCSEL ── GND
        │
       J1 Pin 1 ── VIN
       J1 Pin 2 ── GND
       J1 Pin 3 ── SCL  (host side)
       J1 Pin 4 ── SDA  (host side)
       J1 Pin 5 ── XSHUT
       J1 Pin 6 ── GPIO1


═══════════════════════════════════════════════════════════════════════════════════
 COMPONENT SUMMARY
═══════════════════════════════════════════════════════════════════════════════════

  Ref   Value          Purpose
  ────  ─────────────  ──────────────────────────────────────────────
  U1    VL53L0X        ST 940 nm ToF ranging sensor (I²C addr 0x29)
  LDO   "663K" SOT23-5 2.8V LDO (XC6206-class), drops VIN → VDD
  Q1A   NMOS (BSS138)  Bidirectional level shifter for SCL
  Q1B   NMOS (BSS138)  Bidirectional level shifter for SDA
  R1    10kΩ           SCL host-side pull-up to VIN
  R2    10kΩ           SDA host-side pull-up to VIN
  R3    10kΩ           SCL 2.8V-side pull-up to VDD
  R4    10kΩ           SDA 2.8V-side pull-up to VDD
  R5    47kΩ           XSHUT pull-up to VDD (2.8V)
  R6    47kΩ           GPIO1 pull-up to VDD (2.8V)
  C1    1µF            LDO input capacitor
  C3    1µF            VDD bulk / decoupling capacitor
  C4    4.7µF          VDD bulk capacitor
  J1    6-pin header   VIN, GND, SCL, SDA, XSHUT, GPIO1


═══════════════════════════════════════════════════════════════════════════════════
 INTEGRATION NOTES
═══════════════════════════════════════════════════════════════════════════════════

  - Power: VIN accepts 3.3–5V; the on-board LDO regulates to 2.8V for
    the VL53L0X. Do NOT feed 2.8V directly into VIN — the LDO needs
    headroom. Use 3.3V or 5V.
  - I²C: SCL and SDA on the header are already level-shifted and
    pulled up to VIN. They can be wired directly to a 3.3V or 5V I²C
    bus. No external pull-ups are required for short runs.
  - I²C address is fixed at 0x29. To use multiple sensors, sequence
    them via XSHUT and re-program addresses at runtime.
  - XSHUT: pulled HIGH (2.8V) by R5. To shut the sensor down, drive
    XSHUT LOW from a GPIO. To release, set the GPIO to INPUT (high-Z)
    and let R5 pull it HIGH — this keeps the GPIO from forcing 5V/3.3V
    onto a 2.8V net.
  - GPIO1: VL53L0X interrupt output, open-drain, pulled HIGH (2.8V).
    Wire to any digital input or interrupt-capable pin. The 2.8V HIGH
    level is above the V_IH threshold of a 5V Arduino Mega's TTL inputs.
  - Mounting: keep the cover-glass area clean and free of obstructions
    within ~10 mm of the sensor for accurate ranging.
```
