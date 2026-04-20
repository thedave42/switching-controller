# MB85RC256V FRAM Breakout Board — Circuit Diagram

Reference schematic for the Adafruit MB85RC256V I2C FRAM breakout board
(Adafruit P/N 1895, "FRAM_SO8_I2C_REV-B") used for non-volatile state
persistence on the switching controller (see `circuit-diagram.instructions.md`
for how it fits into the system, and the FRAM Persistence section of
`copilot-instructions.md` for usage details).

```
╔══════════════════════════════════════════════════════════════════════════════════╗
║              MB85RC256V I2C FRAM BREAKOUT BOARD (Adafruit)                     ║
╚══════════════════════════════════════════════════════════════════════════════════╝


═══════════════════════════════════════════════════════════════════════════════════
 1. MAIN IC — MB85RC256V (U1)
═══════════════════════════════════════════════════════════════════════════════════

  32 KB (256 Kbit) ferroelectric RAM, I²C interface, SOIC-8 package.

                            ┌───────────────────┐
                    A0  ── 1 │ A0           VCC │ 8 ── VCC
                    A1  ── 2 │ A1   MB85RC   WP │ 7 ── WP
                    A2  ── 3 │ A2   256V    SCL │ 6 ── SCL
                    GND ── 4 │ GND          SDA │ 5 ── SDA
                            └───────────────────┘


═══════════════════════════════════════════════════════════════════════════════════
 2. I²C ADDRESS
═══════════════════════════════════════════════════════════════════════════════════

  Base address: 0b1010_A2A1A0 → range 0x50–0x57.
  Address pins A0/A1/A2 have internal pull-downs; leaving them
  unconnected (the default on this breakout) yields address 0x50.

  In this project all three address pins are left floating → address 0x50.


═══════════════════════════════════════════════════════════════════════════════════
 3. POWER & DECOUPLING
═══════════════════════════════════════════════════════════════════════════════════

             VCC ──────┬───────────────── U1 Pin 8 (VCC)
                       │
                  ┌────┴────┐
                  │  C1     │  0.1µF ceramic (decoupling)
                  │  0.1µF  │
                  └────┬────┘
                       │
             GND ──────┴───────────────── U1 Pin 4 (GND)


═══════════════════════════════════════════════════════════════════════════════════
 4. I²C PULL-UPS (SDA / SCL)
═══════════════════════════════════════════════════════════════════════════════════

  The breakout includes 10kΩ pull-ups on SDA and SCL to VCC. These are
  fine for short (<30 cm) breadboard runs at 100 kHz. If the FRAM shares
  the bus with other pulled-up devices, the parallel pull-up value will
  drop accordingly — verify the bus capacitance budget.

             VCC                      VCC
              │                        │
           ┌──┴──┐                  ┌──┴──┐
           │ R1  │  10kΩ            │ R2  │  10kΩ
           │ 10K │                  │ 10K │
           └──┬──┘                  └──┬──┘
              │                        │
              ├── U1 Pin 5 (SDA)       ├── U1 Pin 6 (SCL)
              │                        │
              └── JP1 Pin 5 (SDA)      └── JP1 Pin 4 (SCL)


═══════════════════════════════════════════════════════════════════════════════════
 5. WRITE PROTECT (WP)
═══════════════════════════════════════════════════════════════════════════════════

  The WP pin (U1 pin 7) controls write protection:
    - WP HIGH → writes are blocked (read-only).
    - WP LOW  → writes are enabled.

  WP has an internal pull-down inside the MB85RC256V, so leaving the pin
  unconnected defaults to write-enabled. The pin is broken out to JP1 so
  it can be pulled HIGH externally to lock the FRAM contents.

  In this project WP is left unconnected (writes enabled).

             U1 Pin 7 (WP) ─────────── JP1 Pin 3 (WP)
                       │
                       │  internal pull-down to GND inside U1
                       │  (writes enabled by default)


═══════════════════════════════════════════════════════════════════════════════════
 6. BREAKOUT HEADER — JP1 (8-pin)
═══════════════════════════════════════════════════════════════════════════════════

  Single 8-pin 0.1" header along the long edge of the board. Pin order
  matches the silkscreen on the Adafruit board.

  ┌────────────────────────────┐
  │  JP1 — 8 Pin               │
  │                            │
  │  Pin 1 ── VCC              │  ── U1 Pin 8 (and pull-up rail)
  │  Pin 2 ── GND              │  ── U1 Pin 4
  │  Pin 3 ── WP               │  ── U1 Pin 7 (internal pull-down)
  │  Pin 4 ── SCL              │  ── U1 Pin 6, with R2 10kΩ pull-up
  │  Pin 5 ── SDA              │  ── U1 Pin 5, with R1 10kΩ pull-up
  │  Pin 6 ── A0               │  ── U1 Pin 1 (internal pull-down)
  │  Pin 7 ── A1               │  ── U1 Pin 2 (internal pull-down)
  │  Pin 8 ── A2               │  ── U1 Pin 3 (internal pull-down)
  │                            │
  └────────────────────────────┘


═══════════════════════════════════════════════════════════════════════════════════
 FULL BOARD SCHEMATIC
═══════════════════════════════════════════════════════════════════════════════════

                     VCC
                      │
        ┌─────────────┼──────────────────────────────────────┐
        │             │                                      │
   ┌────┴────┐        │                                      │
   │C1 0.1µF │        │                                      │
   └────┬────┘        │                                      │
        │             │                                      │
       GND            │                                      │
                      │                                      │
                      │   ┌──────────────────────────────┐  │
                      │   │                              │  │
                      ├── R1 10kΩ ──┬─ U1 Pin 5 (SDA) ───┼──┤
                      ├── R2 10kΩ ──┼─ U1 Pin 6 (SCL) ───┼──┤
                      │             │                    │  │
                      │             │      MB85RC256V    │  │
                      │             │      (U1)          │  │
                      │             │                    │  │
       JP1.5 ── SDA ──┼─────────────┘                    │  │
       JP1.4 ── SCL ──┼──────────────┐                   │  │
                      │              └─ U1 Pin 6         │  │
       JP1.3 ── WP ───┼─────────────── U1 Pin 7          │  │
                      │                (internal         │  │
                      │                 pull-down)       │  │
                      │                                  │  │
       JP1.6 ── A0 ───┼─────────────── U1 Pin 1          │  │
       JP1.7 ── A1 ───┼─────────────── U1 Pin 2          │  │
       JP1.8 ── A2 ───┼─────────────── U1 Pin 3          │  │
                      │                (all three have   │  │
                      │                 internal         │  │
                      │                 pull-downs)      │  │
                      │                                  │  │
       JP1.1 ── VCC ──┴───────────────── U1 Pin 8 ───────┘  │
                                                            │
       JP1.2 ── GND ─────────────────── U1 Pin 4 ───────────┘


═══════════════════════════════════════════════════════════════════════════════════
 COMPONENT SUMMARY
═══════════════════════════════════════════════════════════════════════════════════

  Ref   Value        Purpose
  ────  ──────────   ──────────────────────────────────────────────
  U1    MB85RC256V   32 KB I²C FRAM (256 Kbit, SOIC-8)
  R1    10kΩ         SDA pull-up to VCC
  R2    10kΩ         SCL pull-up to VCC
  C1    0.1µF        VCC decoupling capacitor (ceramic)
  JP1   8-pin header VCC, GND, WP, SCL, SDA, A0, A1, A2


═══════════════════════════════════════════════════════════════════════════════════
 NOTES FOR SWITCHING CONTROLLER PROJECT
═══════════════════════════════════════════════════════════════════════════════════

  - VCC is tied to the Mega's 5V rail; the MB85RC256V tolerates 2.7–5.5 V.
  - GND is common with the Mega.
  - A0/A1/A2 are left unconnected → I²C address 0x50.
  - WP is left unconnected → writes enabled (the firmware writes after
    every motor pulse and on setup-mode confirmation).
  - The FRAM lives on a private SoftwareWire bus on Mega pins
    14 (SDA) and 15 (SCL) — NOT the hardware TWI pins (20/21), which
    are reserved for the DCC-EX I²C slave role at 0x65.
        - JP1 Pin 5 (SDA) → Mega pin 14
        - JP1 Pin 4 (SCL) → Mega pin 15
        - JP1 Pin 1 (VCC) → Mega 5V
        - JP1 Pin 2 (GND) → Mega GND
  - Because this is a private bus with only one device, the on-board
    10kΩ pull-ups (R1/R2) are sufficient; no external pull-ups are
    needed on pins 14/15.
  - Only 51 bytes of the 32 KB capacity are used (magic + version +
    12 turnouts × 4 bytes + CRC8). FRAM has ~10¹³ write endurance, so
    no wear-leveling or write-skipping is required.
  - To enable hardware write protection, tie JP1 Pin 3 (WP) to VCC.
```
