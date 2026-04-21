# EX-CSB1 DCC Command Station / Booster — Circuit Diagram

Reference schematic for the DCC-EX **EX-CSB1** open-source command-station /
booster board. Derived directly from the KiCad schematic files
(`EX-CSB1.kicad_sch` and `power.kicad_sch`) in the
[DCC-EX/EX-CSB1](https://github.com/DCC-EX/EX-CSB1) repository.

The board is built around an ESP32-WROOM-32E module driving two TI DRV8874
H-bridge motor drivers (one per output: MAIN and PROG / Booster A and B). It
is USB-C powered (CH340C USB↔serial bridge) with a separate barrel-jack
track-power input regulated down to +5V (TPS54302 buck) and +3.3V
(AMS1117 LDO). Arduino UNO–compatible header footprints (J107, J108, J112,
J113) expose ESP32 GPIOs in the standard UNO arrangement so existing shields
fit, while three I²C connectors (Stemma QT, OLED 4-pin, dual 2×4) and an
opto-isolated booster-sync input round out the I/O.

```
╔══════════════════════════════════════════════════════════════════════════════════╗
║              EX-CSB1 — DCC-EX OPEN-SOURCE COMMAND STATION / BOOSTER             ║
╚══════════════════════════════════════════════════════════════════════════════════╝


═══════════════════════════════════════════════════════════════════════════════════
 PHYSICAL BOARD LAYOUT (top view, component side)
═══════════════════════════════════════════════════════════════════════════════════

  Derived from the KiCad PCB file (EX-CSB1.kicad_pcb).
  Board dimensions: ~66 × 53 mm, rounded corners.
  All connector positions and orientations reflect the actual PCB placement.

  Arduino UNO–compatible shield headers run along the top and bottom edges
  (J107/J113 on top, J108/J204/J112 on bottom). The USB-C connector and
  reset button are on the right edge. Screw terminals (J101, J103, J110)
  and the Stemma QT connector (J105) are on the left side.

   TOP EDGE
   ┌───────────────────────────────────────────────────────────────────┐
   │ J110      J111   J107         JP1/2  J106       J113              │
   │ Booster   OLED   DigHigh 10p  SDA/   DualI2C    DigLow 8p         │
   │ Sync 2p   I2C    ═══════════  SCL    2×4 hdr    ═══════════       │
   │ ┌──┐      4p     [scl..d8]   jumpers [+3V3      [d7..d0]          │
   │ │BC│      ┌──┐                       sda gnd                      │
   │ │BD│      │  │                        scl]                        │
   │ └──┘      └──┘                                  U105 U106         │
   │                     U102                         USBLC6 CH340     │
   │ J105              DRV8874      U202                    Q101       ├─ J114
   │ QWiic            (MAIN/A)     AMS1117             D105○ R/SW      ├ USB-C
   │ ┌──┐                                            (blue) ┌─┐        │
   │ └──┘                                                   └─┘        │
   │                                                  SW101 reset      │
   │ J101               U101                                           │
   │ MAIN              DRV8874      D202○ D205○                        │
   │ Track             (PROG/B)     (+5V)  (+3V3)                      │
   │ ┌──┐                                                              │
   │ │A1│                           U104                               │
   │ │A2│                          TLP2361            U103             │
   │ └──┘                                       ESP32-WROOM-32E        │
   │                                             ┌─────────────┐       │
   │ J103                                        │             │       │
   │ PROG                           L201         │   ESP32     │       │
   │ Track            Q201          15µH         │   module    │       │
   │ ┌──┐            AO4407C                     │             │       │
   │ │B1│                           U201         │             │       │
   │ │B2│                          TPS54302      │             │       │
   │ └──┘                          (buck)        └─────────────┘       │
   │            J201                                                   │
   │         Barrel Jack                                               │
   │         (DC input)  C201–C203 bulk                                │
   │             ┌─┐                                                   │
   │             └─┘                                                   │
   │         J108          J204        J112                            │
   │         Power 8p      VM tap 4p   Analog 6p                       │
   │         ═══════════   ═══════     ═══════════                     │
   │         [ioref..vin]  [VM×3 GND]  [a0..a5]                        │
   └───────────────────────────────────────────────────────────────────┘
   BOTTOM EDGE

  Legend:
    ○ = status LED       ┌──┐ = Phoenix screw terminal (2-pin, side-entry)
    ┌─┐ = tactile switch ═══ = pin socket header (through-hole)
    ┌──┐ = JST connector  2×4 hdr = dual-row pin header

  Connector quick-reference (pin 1 marked on PCB silkscreen):
    J101 (MAIN):  B1=out1a, B2=out2a     J103 (PROG):  B1=out1b, B2=out2b
    J110 (Boost): B-C, B-D               J105 (QWiic): GND,+3V3,SDA,SCL
    J111 (OLED):  GND,+3V3,SCL,SDA       J114 (USB-C): USB 2.0 data + VBUS
    J201 (DC in): centre=+V, sleeve=GND  J204 (VM):    VM×3, GND
    SW101: manual reset (active LOW)      JP101/JP102: I²C pull-up enables


═══════════════════════════════════════════════════════════════════════════════════
 1. MAIN MCU — ESP32-WROOM-32E (U103)
═══════════════════════════════════════════════════════════════════════════════════

  Espressif ESP32-WROOM-32E module (38-pin SMD castellated module).
  Powered from +3.3V (pin 2 VDD). All four GND pins (1, 15, 38, 39) tied
  to GND. EN (pin 3) is the system reset; IO0 (pin 25) is the boot-mode
  strap. Both EN and IO0 are driven by the auto-program circuit Q101
  (see §6).

                            ┌────────────────────────────────┐
                  GND ─── 1 │ GND                        GND │ 38 ── GND
                  VDD ─── 2 │ VDD                        GND │ 39 ── GND
                  EN  ─── 3 │ EN  (reset_n)              GND │ 15 ── GND
            SENSOR_VP ── 4  │ SENSOR_VP (a0/sen_c)     TXD0  │ 35 ── USB_RX (net)
            SENSOR_VN ── 5  │ SENSOR_VN (a1/sen_d)     RXD0  │ 34 ── USB_TX (net)
                IO34  ── 6  │ IO34 (sen_a)              IO0  │ 25 ── IO0_boot_option
                IO35  ── 7  │ IO35 (sen_b)              IO2  │ 24 ── en/in1b
                IO32  ── 8  │ IO32 (Railsync_in)        IO4  │ 26 ── direction_d (d13)
                IO33  ── 9  │ IO33 (d7/esp_led)         IO5  │ 29 ── direction_c (d12)
                IO25  ──10  │ IO25 (nSleepa)           IO12  │ 14 ── brake_d
                IO26  ──11  │ IO26 (pwm_c, d3)         IO13  │ 16 ── brake_c
                IO27  ──12  │ IO27 (nSleepb)           IO14  │ 13 ── en/in1a
                IO14  ──13  │ (also en/in1a)           IO15  │ 23 ── ph/in2b
                IO12  ──14  │ (also brake_d)           IO16  │ 27 ── pwm_d (d11)
                IO13  ──16  │ (also brake_c)           IO17  │ 28 ── a5 / fault_n_d
              SHD/SD2 ──17  │ flash                    IO18  │ 30 ── a4 / fault_n_c
              SWP/SD3 ──18  │ flash                    IO19  │ 31 ── fault_n_a
              SCS/CMD ──19  │ flash                    IO21  │ 33 ── sda
              SCK/CLK ──20  │ flash                    IO22  │ 36 ── scl
              SDO/SD0 ──21  │ flash                    IO23  │ 37 ── fault_n_b
              SDI/SD1 ──22  │ flash                     NC   │ 32 ── (no connect)
                            └────────────────────────────────┘

  Note: many ESP32 GPIOs serve a dual role — they are wired to both an
  Arduino-style header pin (e.g. J107 d11) and an internal peripheral
  (e.g. pwm_d on motor driver U101). The labels above show both names
  where applicable. The USB_TX/USB_RX net labels follow the CH340C
  (UART-bridge) perspective: USB_TX is the CH340C transmit line (→ ESP32
  RXD0 pin 34), USB_RX is the CH340C receive line (→ ESP32 TXD0 pin 35).


═══════════════════════════════════════════════════════════════════════════════════
 2. POWER TREE
═══════════════════════════════════════════════════════════════════════════════════

  Two independent power inputs feed two ORed downstream rails:

  - VBUS (USB-C, +5V from host) → Schottky D204 → +5V_vreg
  - VM   (barrel jack, ~12–24V DC, after reverse-polarity P-MOSFET) →
         buck U201 (TPS54302) → +5V → Schottky D203 → +5V_vreg
  - +5V_vreg → LDO U202 (AMS1117-3.3) → +3.3V

  This means the ESP32 (3.3V domain) can be powered from USB alone for
  flashing/testing without track power. The motor drivers, however,
  always need VM.

  ┌──────────────────────────────────────────────────────────────────────┐
  │                       BARREL JACK INPUT (J201)                       │
  │                                                                      │
  │  +V (centre)                                                         │
  │   │                                                                  │
  │   ├── R201 100kΩ ──┐                                                 │
  │   │                ├── Q201 gate (AO4407C, P-MOSFET, reverse-polarity│
  │   │                │   protection, drain = +V input, source = VM)    │
  │   │                D201 (10V Zener, gate clamp)                      │
  │   │                                                                  │
  │   └── (Q201 drain; source → VM rail)                                 │
  │                                                                      │
  │  GND (sleeve) ── GND                                                 │
  └──────────────────────────────────────────────────────────────────────┘
                              │
                              VM
                              │
  ┌──────────────────────────────────────────────────────────────────────┐
  │                   TPS54302 SYNCHRONOUS BUCK (U201)                   │
  │                                                                      │
  │   VIN (pin 3) ── VM                                                  │
  │   GND (pin 1) ── GND                                                 │
  │   EN  (pin 5) ── (left to its internal default; not driven by ESP32) │
  │   FB  (pin 4) ── feedback divider R202 (100k top) / R203 (13k3 btm)  │
  │                  + C209 (56pF) feed-forward cap                      │
  │   SW  (pin 2) ── L201 15µH ── +5V rail                               │
  │   BOOT(pin 6) ── C204 → SW (bootstrap cap)                           │
  │                                                                      │
  │   Input bulk caps : C205 10µF VM, C206 100n VM                       │
  │   Output bulk caps: C207 47µF, C208 47µF (on +5V rail)               │
  │                                                                      │
  │   D202 (LED + R204 13k3) — "+5V present" status LED                  │
  └──────────────────────────────────────────────────────────────────────┘
                              │
                              +5V
                              │
                       ┌──────┴──────┐
                       │             │
                  D203 (Schottky)   D204 (Schottky)
                  A=+5V, K=+5V_vreg A=VBUS, K=+5V_vreg
                       │             │
                       └──────┬──────┘
                              │
                            +5V_vreg
                              │
                              C210 47µF (bulk / decoupling)
                              │
  ┌──────────────────────────────────────────────────────────────────────┐
  │                     AMS1117-3.3 LDO (U202)                           │
  │                                                                      │
  │   VI  (pin 3) ── +5V_vreg                                            │
  │   GND (pin 1) ── GND                                                 │
  │   VO  (pin 2) ── +3V3 rail (with C211 47µF + R205 5k1 + D205 LED     │
  │                              "+3V3 present" status indicator)        │
  └──────────────────────────────────────────────────────────────────────┘
                              │
                              +3V3 rail
                              │
       (powers ESP32 VDD, motor-driver VRef pins, opto VCC, CH340 V3,
        I²C pull-ups via JP101/JP102, all on-board pull-up rails.)


═══════════════════════════════════════════════════════════════════════════════════
 3. USB INTERFACE — USB-C → ESD → CH340C
═══════════════════════════════════════════════════════════════════════════════════

  USB-C receptacle (J114, USB 2.0 only). VBUS (5V) is fed into the
  +5V_vreg ORing diode (D204) and into U105's VBUS pin. The differential
  pair D+/D- passes through U105 (USBLC6-2SC6 ESD/EMI clamp) and on to
  CH340C (U106) which presents itself to the host as a USB-serial port
  and exposes TXD/RXD plus modem control lines (DTR, RTS) to the ESP32.

  ┌──────────────────┐                                  ┌─────────────────┐
  │  USB-C  (J114)   │                                  │  USBLC6-2SC6    │
  │                  │     +5V (to D204 / +5V_vreg)     │       (U105)    │
  │  A4/A9 VBUS ─────┼──────────────────────────────────┤ VBUS (pin 5)    │
  │  B4/B9 VBUS ─────┤                                  │                 │
  │                  │                                  │  I/O1 (1,6) ────┼── usb_d+
  │  A6/B6 D+   ─────┼──────────────────────────────────┤ I/O1            │
  │  A7/B7 D-   ─────┼──────────────────────────────────┤ I/O2 (3,4) ────┼── usb_d-
  │                  │                                  │  GND  (pin 2) ── GND
  │  A5    CC1  ─────┼──── R119 5k1 ── GND (pull-down,  └─────────────────┘
  │                  │                  CC pull-down for                     │
  │  B5    CC2  ─────┼──── R118 5k1 ── GND  USB-C UFP role)                 │
  │                  │                                                       │
  │  A1/A12/B1/B12   │                                                       │
  │  GND, S1 SHIELD ─┼── GND                                                 │
  └──────────────────┘                                                       │
                                                                             │
                         usb_d+, usb_d-                                      │
                              │                                              │
                              ▼                                              │
                       ┌───────────────────────────────────────────────────────────┐
                       │       CH340C  USB↔Serial (U106)                           │
                       │                                                           │
                       │  VCC  (16) ── +3V3                                        │
                       │  V3   (4)  ── tied to VCC (+3V3); internal LDO not used.  │
                       │              C120 on V3/VCC to GND for stability.         │
                       │  GND  (1)  ── GND                                         │
                       │  UD+  (5)  ── usb_d+                                      │
                       │  UD-  (6)  ── usb_d-                                      │
                       │  TXD  (2)  ── txd ── R120 470Ω ── USB_TX net (→ ESP32 RX) │
                       │  RXD  (3)  ── rxd ── R121 470Ω ── USB_RX net (→ ESP32 TX) │
                       │  ~DTR (13) ── DTR ── Q101 (auto-reset/boot, §6)           │
                       │  ~RTS (14) ── RTS ── Q101 (auto-reset/boot, §6)           │
                       │  ~CTS,~DCD,~RI,~DSR (9–12) — unused                       │
                       │  R232 (15) ── (config; tied via 22k pull, see schematic)  │
                       │  NC   (7,8) — no connect                                  │
                       │                                                           │
                       │  No dedicated TX/RX activity LEDs on the schematic.       │
                       └───────────────────────────────────────────────────────────┘


═══════════════════════════════════════════════════════════════════════════════════
 4. MOTOR DRIVERS — DUAL DRV8874 H-BRIDGES (U101, U102)
═══════════════════════════════════════════════════════════════════════════════════

  Two TI DRV8874PWPR H-bridge motor drivers, one per DCC output. Each
  driver supplies a screw-terminal output (J101 for U102 = MAIN/A,
  J103 for U101 = PROG/B) plus current sense feedback to the ESP32.

  Per the schematic the input-mode strap PMODE is tied to GND on both
  drivers — this puts the DRV8874 into PH/EN (phase + enable) input
  mode. EN/IN1 is the "enable / direction-1" input and PH/IN2 is the
  "phase / direction-2" input. IMODE is also tied to GND (analog
  current-feedback mode on IPROPI).

  ┌──────────────────────────────────────────────────────────────────────┐
  │                    DRV8874 (U101) — PROG / Booster B                 │
  │                                                                      │
  │  EN/IN1 (1)  ── en/in1b   ── ESP32 IO2                               │
  │  PH/IN2 (2)  ── ph/in2b   ── ESP32 IO15                              │
  │  nSleep (3)  ── nSleepb   ── ESP32 IO27, with R109 100k pull-down GND│
  │  nFAULT (4)  ── fault_n_b ── ESP32 IO23, with R105 100k pull-up +3V3 │
  │  VRef   (5)  ── +3V3                                                 │
  │  IPROPI (6)  ── sen_b ── ESP32 IO35 (ADC), with                      │
  │                            R110 1k to GND  +  C112 22n filter        │
  │                            R106 22k to +3V3 (bias)                   │
  │  IMODE  (7)  ── GND                                                  │
  │  OUT1   (8)  ── out1b ── J103 pin 1                                  │
  │  GND    (9,15,17) ── GND                                             │
  │  OUT2   (10) ── out2b ── J103 pin 2                                  │
  │  VM     (11) ── VM rail (with C103 10µF + C104 100n decoupling)      │
  │  VCP    (12) ── vcpb ── C107 100n between VM (pin 11) and VCP (pin   │
  │                  12) — charge-pump bootstrap cap, NOT GND-referenced │
  │  CPH    (13) ── C110 ── CPL  (charge-pump fly cap)                   │
  │  CPL    (14) ──   ┘                                                  │
  │  PMODE  (16) ── GND  (selects PH/EN input mode)                      │
  │                                                                      │
  │  D103 + D104 — antiparallel 0603 yellow LEDs in series with R102     │
  │  (22k) across out1b/out2b. Acts as a direction indicator: one LED    │
  │  lights when the H-bridge drives one polarity, the other LED lights  │
  │  when it drives the opposite polarity.                               │
  └──────────────────────────────────────────────────────────────────────┘

  ┌──────────────────────────────────────────────────────────────────────┐
  │                    DRV8874 (U102) — MAIN / Booster A                 │
  │                                                                      │
  │  EN/IN1 (1)  ── en/in1a   ── ESP32 IO14                              │
  │  PH/IN2 (2)  ── ph/in2a   ── ESP32 IO0  (NOTE: shared with boot      │
  │                              strap. Held HIGH at boot by Q101 /      │
  │                              R-network, see §6.)                     │
  │  nSleep (3)  ── nSleepa   ── ESP32 IO25, with R104 100k pull-down GND│
  │  nFAULT (4)  ── fault_n_a ── ESP32 IO19, with R107 100k pull-up +3V3 │
  │  VRef   (5)  ── +3V3                                                 │
  │  IPROPI (6)  ── sen_a ── ESP32 IO34 (ADC), with                      │
  │                            R111 1k to GND + C113 22n filter          │
  │                            R108 22k to +3V3 (bias)                   │
  │  IMODE  (7)  ── GND                                                  │
  │  OUT1   (8)  ── out1a ── J101 pin 1                                  │
  │  GND    (9,15,17) ── GND                                             │
  │  OUT2   (10) ── out2a ── J101 pin 2                                  │
  │  VM     (11) ── VM (with C105 10µF + C106 100n decoupling)           │
  │  VCP    (12) ── vcpa ── C108 100n between VM (pin 11) and VCP (pin   │
  │                  12) — charge-pump bootstrap cap, NOT GND-referenced │
  │  CPH    (13) ── C111 ── CPL (charge-pump fly cap, 22n)               │
  │  CPL    (14) ──   ┘                                                  │
  │  PMODE  (16) ── GND                                                  │
  │                                                                      │
  │  D101 + D102 — antiparallel 0603 yellow LEDs in series with R101     │
  │  (22k) across out1a/out2a. Direction indicator (mirror of D103/D104  │
  │  on the U101 side).                                                  │
  └──────────────────────────────────────────────────────────────────────┘


═══════════════════════════════════════════════════════════════════════════════════
 5. BOOSTER SYNC INPUT — TLP2361 OPTOCOUPLER (U104)
═══════════════════════════════════════════════════════════════════════════════════

  When the EX-CSB1 is used as a booster (slave to an external command
  station), the upstream RailSync DCC waveform is fed into the J110
  2-pin terminal and isolated through the TLP2361 high-speed optocoupler.
  The opto's logic-side output drives the ESP32's IO32 (Railsync_in).

  ┌──────────────────────────────────────────────────────────────────────┐
  │                                                                      │
  │  J110 (booster sync input, 2-pin terminal)                           │
  │                                                                      │
  │  Verified from netlist:                                              │
  │    Booster-C (J110.1) → D106 cathode AND TLP2361 pin 3 (LED K)       │
  │    Booster-D (J110.2) → R115 (1k) → opto_anode                       │
  │    opto_anode         → D106 anode AND TLP2361 pin 1 (LED A)         │
  │                                                                      │
  │  Forward conduction path (Booster-D positive vs. Booster-C):         │
  │                                                                      │
  │    J110.2 (B-D,+) ── R115 1k ─┬─ opto_anode ─ TLP2361 pin 1 (A)─┐    │
  │                                │                                  │  │
  │                                └─── D106 ────┐  TLP2361 pin 3 (K)│   │
  │                                    (reverse   │                   │  │
  │                                     protect)  └── Booster-C ◄────┘   │
  │                                                                      │
  │  D106 sits in parallel with the opto LED, cathode toward opto_anode  │
  │  and anode toward Booster-C, clamping reverse voltage on the LED if  │
  │  the input polarity is flipped. R115 (1k) limits LED current.        │
  │                                                                      │
  │  TLP2361 pin 4 (GND)   ── GND                                        │
  │  TLP2361 pin 5 (Vout)  ── Railsync_in ── ESP32 IO32                  │
  │  TLP2361 pin 6 (VCC)   ── +3V3                                       │
  └──────────────────────────────────────────────────────────────────────┘


═══════════════════════════════════════════════════════════════════════════════════
 6. AUTO-RESET / AUTO-PROGRAM CIRCUIT — Q101 (UMH3N)
═══════════════════════════════════════════════════════════════════════════════════

  Q101 (UMH3N) is a 3-unit symbol containing dual NPN transistors with
  bias resistors. The CH340C's DTR and RTS lines drive Q101, and Q101's
  collectors connect to the ESP32 EN (reset_n) and IO0 boot-strap nets.
  This appears to implement the standard Espressif "auto-reset /
  auto-bootloader" circuit so esptool can put the chip into download
  mode without manual button presses.

  CAVEAT — Q101 multi-unit pin extraction: the netlist build collapsed
  pins from different Q101 units onto overlapping coordinates, so DTR
  and RTS both appear on Q101 pins {1,2,4,5} in the extracted netlist.
  The exact per-unit DTR→EN and RTS→IO0 mapping is NOT verified from
  this extraction and is inferred from the standard Espressif pattern.

  Verified from the netlist (unambiguously):
  - Q101 pins 3 and 6 connect to the same net as ESP32 IO0 AND to U102
    pin 2 (PH/IN2 of the MAIN motor driver). The combined label is
    "IO0_boot_option / ph/in2a". This means the IO0 boot strap and the
    MAIN driver's direction-2 input share a wire — see the IO0 strap
    note in §4.
  - Q101 pins 3 and 6 also appear on the reset_n net (with SW101, R103,
    C109, J108 pin 3, U103 pin 3). This is consistent with the expected
    EN/IO0 auto-reset topology but does not prove the exact per-unit
    mapping due to the multi-unit pin extraction ambiguity noted above.

  Discrete support around the circuit:
  - reset_n: SW101 push-button to GND (manual reset), R103 13k3 pull-up
    to +3V3, C109 2.2µF EN timing / hold-up cap, plus J108 pin 3
    (Arduino RESET header)
  - IO0_boot_option: ESP32 internal strap, optional Q101 pulldown via
    DTR/RTS sequencing (no standalone BOOT button on this revision)


═══════════════════════════════════════════════════════════════════════════════════
 7. I²C BUS AND CONNECTORS
═══════════════════════════════════════════════════════════════════════════════════

  Single shared I²C bus on ESP32 IO21 (SDA) and IO22 (SCL), available on
  three connectors plus the Arduino header. All connectors run at 3.3V
  logic. Pull-ups to +3.3V are provided on-board through R113 (5k1, SDA)
  and R114 (5k1, SCL), gated by solder-bridge jumpers JP101 (SDA) and
  JP102 (SCL) so they can be removed if many I²C devices add up too much
  pull-up current.

   sda ── R113 5k1 ── JP101 ── +3V3
   scl ── R114 5k1 ── JP102 ── +3V3

   ESP32 IO21 (pin 33) ── sda
   ESP32 IO22 (pin 36) ── scl

  ──────────────────────────────────────────
  J105 — Stemma QT / QWiic (4-pin JST-SH 1mm)
  ──────────────────────────────────────────
   Pin 1 ── GND
   Pin 2 ── +3V3
   Pin 3 ── sda
   Pin 4 ── scl
   MP    ── GND (mounting pin)

  ──────────────────────────────────────────
  J111 — OLED I²C header (4-pin 0.1" socket)
  ──────────────────────────────────────────
   Pin 1 ── GND
   Pin 2 ── +3V3
   Pin 3 ── scl
   Pin 4 ── sda

  ──────────────────────────────────────────
  J106 — Dual I²C header (2×4 pin 0.1")
  ──────────────────────────────────────────
   Pin 1 ── +3V3   │  Pin 2 ── +3V3
   Pin 3 ── sda    │  Pin 4 ── sda
   Pin 5 ── GND    │  Pin 6 ── GND
   Pin 7 ── scl    │  Pin 8 ── scl

  (Two parallel 4-pin I²C taps for daisy-chained devices.)


═══════════════════════════════════════════════════════════════════════════════════
 8. ARDUINO UNO–COMPATIBLE HEADERS
═══════════════════════════════════════════════════════════════════════════════════

  The four headers J107, J108, J112, J113 mirror the standard Arduino
  UNO R3 footprint so existing shields plug in. Each header pin
  multiplexes an ESP32 GPIO with an internal function (e.g. an Arduino
  PWM pin doubles as a motor-driver control line).

  ──────────────────────────────────────────
  J108 — Power header (8-pin)
  ──────────────────────────────────────────
   Pin 1 ── (no connect / IOREF position)
   Pin 2 ── +3V3
   Pin 3 ── reset_n
   Pin 4 ── +3V3
   Pin 5 ── +5V
   Pin 6 ── GND
   Pin 7 ── GND
   Pin 8 ── vin_pin   (Arduino VIN — NOT internally connected to VM
                       or any regulator on this rev; intended as a
                       sense / external-feed pin)

  ──────────────────────────────────────────
  J107 — Digital high header (10-pin)
  ──────────────────────────────────────────
   Pin 1 ── scl       (= ESP32 IO22)
   Pin 2 ── sda       (= ESP32 IO21)
   Pin 3 ── aref      (no internal connection; for shield reference)
   Pin 4 ── GND
   Pin 5 ── d13       (= ESP32 IO4   — also direction_d)
   Pin 6 ── d12       (= ESP32 IO5   — also direction_c)
   Pin 7 ── d11       (= ESP32 IO16  — also pwm_d)
   Pin 8 ── d10       (mapped to an ESP32 GPIO; see schematic)
   Pin 9 ── d9        (= brake_c, ESP32 IO13)
   Pin 10── d8        (= brake_d, ESP32 IO12)

  ──────────────────────────────────────────
  J113 — Digital low header (8-pin)
  ──────────────────────────────────────────
   Pin 1 ── d7        (= ESP32 IO33)
   Pin 2 ── d6
   Pin 3 ── d5
   Pin 4 ── d4
   Pin 5 ── d3        (= pwm_c, ESP32 IO26)
   Pin 6 ── d2
   Pin 7 ── d1        (= USB_TX net; series R120 to CH340C TXD pin 2,
                       so this pin shares with the USB-serial bridge)
   Pin 8 ── d0        (= USB_RX net; series R121 to CH340C RXD pin 3)

  ──────────────────────────────────────────
  J112 — Analog header (6-pin)
  ──────────────────────────────────────────
   Pin 1 ── a0  ── via R116 22k bias to +3V3 ── ESP32 SENSOR_VP (pin 4)
                   (also labeled sen_c on schematic)
   Pin 2 ── a1  ── via R117 22k bias to +3V3 ── ESP32 SENSOR_VN (pin 5)
                   (also labeled sen_d on schematic)
   Pin 3 ── a2  (broken out to header; routing per schematic)
   Pin 4 ── a3
   Pin 5 ── a4  (= ESP32 IO18, also labeled fault_n_c)
   Pin 6 ── a5  (= ESP32 IO17, also labeled fault_n_d)


═══════════════════════════════════════════════════════════════════════════════════
 9. POWER / TRACK CONNECTORS (SCREW TERMINALS)
═══════════════════════════════════════════════════════════════════════════════════

  ──────────────────────────────────────────
  J201 — DC barrel jack (centre-positive, with mounting pin)
  ──────────────────────────────────────────
   Centre pin (1) ── into Q201 reverse-polarity P-MOSFET → VM rail
   Sleeve     (2) ── GND
   MP             ── GND

  ──────────────────────────────────────────
  J204 — VM tap header (4-pin 0.1" socket, on the power sub-sheet)
  ──────────────────────────────────────────
   Pin 1 ── VM
   Pin 2 ── VM
   Pin 3 ── VM
   Pin 4 ── GND
  (Three pins paralleled to VM for an external high-current tap;
   one pin to GND.)

  ──────────────────────────────────────────
  J101 — MAIN / Booster A track output (Phoenix MC 1,5/2-G-3,5)
  ──────────────────────────────────────────
   Pin 1 ── out1a (DRV8874 U102 OUT1)
   Pin 2 ── out2a (DRV8874 U102 OUT2)

  ──────────────────────────────────────────
  J103 — PROG / Booster B track output (Phoenix MC 1,5/2-G-3,5)
  ──────────────────────────────────────────
   Pin 1 ── out1b (DRV8874 U101 OUT1)
   Pin 2 ── out2b (DRV8874 U101 OUT2)

  ──────────────────────────────────────────
  J110 — Booster sync input (Phoenix MC 1,5/2-G-3,5)
  ──────────────────────────────────────────
   Pin 1 ── Booster-C (DCC waveform input — opto LED side)
   Pin 2 ── Booster-D (DCC waveform input — opto LED side, current limited)


═══════════════════════════════════════════════════════════════════════════════════
 10. USER I/O — RESET BUTTON, STATUS LEDs
═══════════════════════════════════════════════════════════════════════════════════

  - SW101 (push button, 45°): one terminal to reset_n, other to GND.
    R103 13k3 pulls reset_n HIGH to +3V3; C109 2.2µF EN timing cap.
  - D101 + D102 (yellow 0603 LEDs, antiparallel) — direction indicator
    across U102 out1a/out2a, in series with R101 (22k).
  - D103 + D104 (yellow 0603 LEDs, antiparallel) — direction indicator
    across U101 out1b/out2b, in series with R102 (22k).
  - D105 (blue 0603 LED) + R112 (3k3) — ESP32 user LED, driven by
    GPIO d7 (net "esp_led"). Not a power-on indicator.
  - D202 (green 0603 LED) + R204 13k3 — "+5V present" indicator (power.kicad_sch)
  - D205 (green 0603 LED) + R205 5k1 — "+3V3 present" indicator (power.kicad_sch)
  - No CH340C TX/RX activity LEDs are present on the schematic.


═══════════════════════════════════════════════════════════════════════════════════
 11. COMPONENT SUMMARY (from EX-CSB1.kicad_sch + power.kicad_sch)
═══════════════════════════════════════════════════════════════════════════════════

  Reference  Value                   Function
  ─────────  ─────────────────────   ──────────────────────────────────────────
  U101       DRV8874PWPR             H-bridge — PROG / Booster B
  U102       DRV8874PWPR             H-bridge — MAIN / Booster A
  U103       ESP32-WROOM-32E         Main MCU (Wi-Fi/BT module)
  U104       TLP2361                 High-speed opto for booster sync input
  U105       USBLC6-2SC6             USB ESD/EMI clamp on D+/D-
  U106       CH340C                  USB ↔ UART bridge
  U201       TPS54302                Synchronous buck (VM → +5V)
  U202       AMS1117-3.3             LDO (+5V_vreg → +3V3)
  Q101       UMH3N                   Dual NPN with bias resistors —
                                     auto-reset / auto-bootloader circuit
  Q201       AO4407C                 P-MOSFET — barrel-jack reverse-polarity
                                     protection
  L201       15µH                    Buck inductor
  D101,D102  0603 yellow LEDs        Antiparallel direction indicator across
                                     U102 (MAIN) outputs, series R101 22k
  D103,D104  0603 yellow LEDs        Antiparallel direction indicator across
                                     U101 (PROG) outputs, series R102 22k
  D105       0603 blue LED           ESP32 user LED on GPIO d7, series R112 3k3
  D106       Schottky                Reverse-protection across booster-sync opto
                                     LED (in series with R115 1k current limit)
  D201       10V Zener               Q201 gate clamp
  D202,D205  0603 green LEDs         "+5V present" / "+3V3 present"
  D203,D204  Schottky                +5V / VBUS ORing diodes into +5V_vreg
  J101       Phoenix 2-pin terminal  MAIN / Booster A track output
  J103       Phoenix 2-pin terminal  PROG / Booster B track output
  J105       Stemma QT / JST-SH 4    I²C
  J106       2×4 0.1" header         Dual I²C
  J107       1×10 0.1" socket        Arduino digital high
  J108       1×8  0.1" socket        Arduino power
  J110       Phoenix 2-pin terminal  Booster sync input (DCC RailSync)
  J111       1×4  0.1" socket        OLED I²C
  J112       1×6  0.1" socket        Arduino analog
  J113       1×8  0.1" socket        Arduino digital low
  J114       USB-C receptacle        USB 2.0 host link
  J201       Barrel jack             Track-power input (VM)
  J204       1×4  0.1" socket        VM tap (external high-current feed)
  JP101,JP102 Solder-bridge jumpers  I²C pull-up enables (default closed)
  SW101      SPST push button        Manual reset
  R112       3k3                     D105 (ESP32 user LED) series resistor
  R113,R114  5k1                     I²C SDA / SCL pull-ups (via JP101/102)
  R115       1k                      Booster-sync opto LED current limit
  R101,R102  22k                     Motor-output direction-indicator-LED
                                     series resistors
  R104,R109  100k                    nSleep pull-downs (motor drivers safe at boot)
  R105,R107  100k                    nFAULT pull-ups
  R110,R111  1k                      IPROPI sense resistors to GND
  R106,R108  22k                     IPROPI bias to +3V3
  R116,R117  22k                     ADC bias on a0/a1 (SENSOR_VP/VN)
  R120,R121  470Ω                    UART series resistors USB↔ESP32
  R201       100k                    Q201 gate pull-up
  R202,R203  100k / 13k3             Buck FB divider
  R204       13k3                    D202 "+5V present" LED current limit
  R205       5k1                     D205 "+3V3 present" LED current limit


═══════════════════════════════════════════════════════════════════════════════════
 12. INTEGRATION NOTES (for the switching-controller project)
═══════════════════════════════════════════════════════════════════════════════════

  - The I²C bus on the EX-CSB1 is **3.3V** logic. Anything driven from
    a 5V Arduino Mega TWI bus (the switching controller) must go
    through a bidirectional level shifter before joining this bus.
  - On-board I²C pull-ups (R113/R114 to +3.3V via JP101/JP102) are
    sufficient for short runs. If you add a TCA9548A or many devices
    you may want to lift those jumpers to avoid over-pulling the bus.
  - The +3V3 rail is regulated from EITHER USB VBUS or VM, so the ESP32
    boots and runs the I²C bus even with no track power applied —
    useful for I²C bring-up before powering the layout.
  - Note that several "Arduino" header pins on this board are actually
    in active use by on-board peripherals (e.g. d11 = pwm_d, d12 =
    direction_c, d13 = direction_d, d3 = pwm_c, d8/d9 = brake_d/c,
    d0/d1 = USB-serial). Check the §1/§8 maps before assuming a header
    pin is free.
  - DCC track output is on the 2-pin Phoenix terminals J101 (MAIN) and
    J103 (PROG). Booster RailSync input (when running as a booster) is
    on J110.
```
