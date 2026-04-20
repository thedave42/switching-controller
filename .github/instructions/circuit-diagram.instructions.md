# Circuit Diagram — Switching Controller

All connections between the Arduino Mega 2560 and external components.
Pin assignments are defined in `include/pinLayout.h`.

```
╔══════════════════════════════════════════════════════════════════════════════════╗
║                    SWITCHING CONTROLLER — FULL CIRCUIT DIAGRAM                  ║
║                    Arduino Mega 2560 + All Peripherals                          ║
╚══════════════════════════════════════════════════════════════════════════════════╝


                         ┌─────────────────────────┐
                         │    ARDUINO MEGA 2560     │
                         │                         │
                         │  5V ──┬─────────────────┼──── +5V Rail
                         │  GND ─┼─────────────────┼──── GND Rail
                         │       │                 │
                         │  Pin 2  (INT4) ─────────┼──── Encoder CLK
                         │  Pin 3  (INT5) ─────────┼──── Encoder DT
                         │  Pin 4  ────────────────┼──── BTN COL2
                         │  Pin 5  ────────────────┼──── BTN COL1
                         │  Pin 6  ────────────────┼──── BTN COL0
                         │  Pin 7  ────────────────┼──── BTN ROW3 (INPUT_PULLUP)
                         │  Pin 8  ────────────────┼──── Encoder SW (INPUT_PULLUP)
                         │  Pin 9  ──── [330Ω] ───┼──── LED Data In (PL9823 chain)
                         │  Pin 10 ────────────────┼──── BTN ROW2 (INPUT_PULLUP)
                         │  Pin 11 ────────────────┼──── BTN ROW1 (INPUT_PULLUP)
                         │  Pin 12 ────────────────┼──── BTN ROW0 (INPUT_PULLUP)
                         │                         │
                         │  Pin 14 ────────────────┼──── FRAM SDA (sw I2C, MB85RC256V)
                         │  Pin 15 ────────────────┼──── FRAM SCL (sw I2C, MB85RC256V)
                         │                         │
                         │  Pin 22 ────────────────┼──── T0 IN1 ──┐
                         │  Pin 23 ────────────────┼──── T0 IN2 ──┤ L298N
                         │  Pin 24 ────────────────┼──── T1 IN1 ──┤ Motor
                         │  Pin 25 ────────────────┼──── T1 IN2 ──┤ Drivers
                         │  Pin 26 ────────────────┼──── T2 IN1 ──┤
                         │  Pin 27 ────────────────┼──── T2 IN2 ──┤
                         │  Pin 28 ────────────────┼──── T3 IN1 ──┤
                         │  Pin 29 ────────────────┼──── T3 IN2 ──┤
                         │  Pin 30 ────────────────┼──── T4 IN1 ──┤
                         │  Pin 31 ────────────────┼──── T4 IN2 ──┤
                         │  Pin 32 ────────────────┼──── T7 IN1 ──┤ (non-sequential
                         │  Pin 33 ────────────────┼──── T7 IN2 ──┤  to match wiring)
                         │  Pin 34 ────────────────┼──── T5 IN1 ──┤
                         │  Pin 35 ────────────────┼──── T5 IN2 ──┤
                         │  Pin 36 ────────────────┼──── T6 IN1 ──┤
                         │  Pin 37 ────────────────┼──── T6 IN2 ──┤
                         │  Pin 38 ────────────────┼──── T9 IN1 ──┤
                         │  Pin 39 ────────────────┼──── T9 IN2 ──┤
                         │  Pin 40 ────────────────┼──── T10 IN1 ─┤
                         │  Pin 41 ────────────────┼──── T10 IN2 ─┤
                         │  Pin 42 ────────────────┼──── T8 IN1 ──┤
                         │  Pin 43 ────────────────┼──── T8 IN2 ──┤
                         │  Pin 44 ────────────────┼──── T11 IN1 ─┤
                         │  Pin 45 ────────────────┼──── T11 IN2 ─┘
                         │                         │
                         │  Pin 46 ────────────────┼──── LCD RS
                         │  Pin 47 ────────────────┼──── LCD E
                         │  Pin 48 ────────────────┼──── LCD D4
                         │  Pin 49 ────────────────┼──── LCD D5
                         │  Pin 50 ────────────────┼──── LCD D6
                         │  Pin 51 ────────────────┼──── LCD D7
                         │                         │
                         └─────────────────────────┘


═══════════════════════════════════════════════════════════════════════════════════
 1. PL9823 LED CHAIN (36 LEDs)
═══════════════════════════════════════════════════════════════════════════════════

                                              LED Power Rail
                                          +5V ────┬────────────────────────
                                                   │
                                              ┌────┴────┐
                                              │470-1000µF│  Electrolytic cap
                                              │   6.3V+  │  as close to first
                                              └────┬────┘  LED as possible
                                                   │
                                          GND ────┴────────────────────────

  Arduino Pin 9 ──── [330Ω] ──── Data In ┤LED 0├─DO── DI┤LED 1├─DO── ... ──DI┤LED 35├
                                              │          │                        │
                                             5V         5V                       5V
                                              │          │                        │
                                             GND        GND                      GND

  Each LED has 4 pins: VCC, GND, Data In (DI), Data Out (DO)
  LEDs are daisy-chained: DO of one connects to DI of the next
  All VCC pins connect to +5V rail; all GND pins connect to GND rail


═══════════════════════════════════════════════════════════════════════════════════
 2. BUTTON MATRIX (4×3 with anti-ghosting diodes)
═══════════════════════════════════════════════════════════════════════════════════

  See docs/button-matrix-circuit.txt for detailed schematic.

              COL0 (Pin 6)      COL1 (Pin 5)      COL2 (Pin 4)
                   │                 │                 │
  ROW0 (Pin 12) ──┤├─▷|────────┤├─▷|────────┤├─▷|───┤  BTN 0,1,2
  ROW1 (Pin 11) ──┤├─▷|────────┤├─▷|────────┤├─▷|───┤  BTN 3,4,5
  ROW2 (Pin 10) ──┤├─▷|────────┤├─▷|────────┤├─▷|───┤  BTN 6,7,8
  ROW3 (Pin 7)  ──┤├─▷|────────┤├─▷|────────┤├─▷|───┤  BTN 9,10,11

  Row pins: INPUT_PULLUP (idle HIGH)
  Column pins: scanned OUTPUT (driven LOW during scan)
  Diodes: anode on row side, cathode on column side (1N4148)


═══════════════════════════════════════════════════════════════════════════════════
 3. ROTARY ENCODER
═══════════════════════════════════════════════════════════════════════════════════

  Encoder        Arduino Mega
  ───────        ────────────
  CLK ────────── Pin 2 (INT4, interrupt-driven)
  DT  ────────── Pin 3 (INT5, interrupt-driven)
  SW  ────────── Pin 8 (INPUT_PULLUP, polled)
  +   ────────── 5V
  -   ────────── GND

  SW connects to GND when pressed (active LOW).
  CLK/DT handled by Paul Stoffregen Encoder library via hardware interrupts.


═══════════════════════════════════════════════════════════════════════════════════
 4. LCD DISPLAY (TC1602A, 4-bit parallel)
═══════════════════════════════════════════════════════════════════════════════════

  TC1602A        Arduino Mega
  ───────        ────────────
  RS  ────────── Pin 46
  E   ────────── Pin 47
  D4  ────────── Pin 48
  D5  ────────── Pin 49
  D6  ────────── Pin 50
  D7  ────────── Pin 51
  VSS ────────── GND
  VDD ────────── 5V
  V0  ────────── Contrast pot wiper (10kΩ pot between 5V and GND)
  RW  ────────── GND (write only)
  A   ────────── 5V (backlight anode, through resistor if needed)
  K   ────────── GND (backlight cathode)


═══════════════════════════════════════════════════════════════════════════════════
 5. L298N MOTOR DRIVERS (H-bridge, 12 turnouts)
═══════════════════════════════════════════════════════════════════════════════════

  Each turnout uses 2 pins (IN1, IN2) on the L298N:
    STRAIGHT = IN1 HIGH, IN2 LOW
    TURN     = IN1 LOW,  IN2 HIGH
    OFF      = IN1 LOW,  IN2 LOW

  Turnout  IN1   IN2   Notes
  ───────  ────  ────  ─────
  T0       22    23
  T1       24    25
  T2       26    27
  T3       28    29
  T4       30    31
  T5       34    35    ┐
  T6       36    37    │ Non-sequential pin assignments
  T7       32    33    │ to match physical wiring layout
  T8       42    43    │
  T9       38    39    │
  T10      40    41    │
  T11      44    45    ┘

  CRITICAL: Motors must never be energized longer than 500ms (MOTOR_DURATION_MS).
  L298N ENA/ENB pins should be tied HIGH or jumpered for always-enabled.
  L298N requires separate motor power supply (typically 12V for turnout motors).


═══════════════════════════════════════════════════════════════════════════════════
 6. FRAM MODULE (MB85RC256V on private software I2C bus)
═══════════════════════════════════════════════════════════════════════════════════

  The hardware TWI bus (pins 20/21) is reserved for the DCC-EX I2C slave
  role at address 0x65, so the FRAM lives on a private bit-banged bus
  driven by SoftwareWire on pins 14/15. Master-only on this bus; the
  Mega is the only master and the FRAM (addr 0x50) is the only slave.

   ┌──────────────────┐                     ┌─────────────────┐
   │  ARDUINO MEGA    │                     │  MB85RC256V     │
   │  2560            │                     │  FRAM Module    │
   │                  │                     │  (Adafruit      │
   │                  │                     │   FRAM_SO8_I2C) │
   │  Pin 14 ─────────┼──── SDA (sw) ───────┤ SDA             │
   │  Pin 15 ─────────┼──── SCL (sw) ───────┤ SCL             │
   │  5V     ─────────┼─────────────────────┤ VCC             │
   │  GND    ─────────┼─────────────────────┤ GND             │
   │                  │                     │ WP  ── GND      │
   │                  │                     │ A0  ── GND      │
   │                  │                     │ A1  ── GND      │
   │                  │                     │ A2  ── GND      │
   │  Pin 20 (SDA) ◄──┼── reserved for DCC-EX (hardware Wire) │
   │  Pin 21 (SCL) ◄──┼── reserved for DCC-EX (hardware Wire) │
   └──────────────────┘                     └─────────────────┘

  I2C address: 0b1010_A2A1A0 = 0x50 with A0/A1/A2 tied LOW.
  A0/A1/A2 have NO internal pull-downs — leaving them floating yields
  an undefined address. They must be wired to GND.

  Pull-up resistors on SDA/SCL: the Adafruit FRAM_SO8_I2C breakout has
  4.7kΩ pull-ups on board. If using a bare chip or a breakout without
  pull-ups, add: SDA ──[4.7kΩ]── 5V and SCL ──[4.7kΩ]── 5V.

  WP (write protect): tied to GND to enable writes. The Adafruit board
  also has an internal pull-down on WP, so leaving it floating would
  default to "writes enabled" too — but on other vendor breakouts WP
  often floats HIGH, so tying it to GND is the portable choice.




  Pin   Function              Pin   Function
  ────  ──────────────────    ────  ──────────────────
  0     (Serial RX)           22    T0 IN1
  1     (Serial TX)           23    T0 IN2
  2     Encoder CLK (INT4)    24    T1 IN1
  3     Encoder DT (INT5)     25    T1 IN2
  4     BTN COL2              26    T2 IN1
  5     BTN COL1              27    T2 IN2
  6     BTN COL0              28    T3 IN1
  7     BTN ROW3              29    T3 IN2
  8     Encoder SW             30    T4 IN1
  9     LED Data              31    T4 IN2
  10    BTN ROW2              32    T7 IN1
  11    BTN ROW1              33    T7 IN2
  12    BTN ROW0              34    T5 IN1
  13    (unused - on-board LED) 35  T5 IN2
  14    FRAM SDA (sw I2C)     36    T6 IN1
  15    FRAM SCL (sw I2C)     37    T6 IN2
  16    (free)                38    T9 IN1
  17    (free)                39    T9 IN2
  18    (free)                40    T10 IN1
  19    (free)                41    T10 IN2
  20    I2C SDA (DCC-EX)      42    T8 IN1
  21    I2C SCL (DCC-EX)      43    T8 IN2
                              44    T11 IN1
  46    LCD RS                45    T11 IN2
  47    LCD E
  48    LCD D4                50    LCD D6
  49    LCD D5                51    LCD D7

  Pins 14-15: software I2C master bus to MB85RC256V FRAM (addr 0x50)
  Pins 16-19: free for future use
  Pins 20-21: hardware TWI, reserved for DCC-EX I2C slave role (addr 0x65)
  Pin 13: avoid for data signals (on-board LED)
```
