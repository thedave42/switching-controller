# Copilot Instructions — Switching Controller

## Project Overview

This is a PlatformIO Arduino project that controls model railroad turnout switches. A physical button matrix lets the operator toggle each turnout between STRAIGHT and TURN positions, with WS2812B LEDs providing visual feedback and an H-bridge motor driver energizing the turnout motor for a timed pulse.

## Hardware

See `.github/instructions/circuit-diagram.instructions.md` for full wiring diagram and pin usage summary.

- **Board:** Arduino Mega 2560 (PlatformIO env `uno`, platform `atmelavr`, board `megaatmega2560`)
- **Button matrix:** 4 rows × 3 columns = 12 buttons
  - Row pins (INPUT_PULLUP): 12, 11, 10, 7
  - Column pins (scanned OUTPUT): 6, 5, 4
  - Anti-ghosting diodes on each cell (anode on row side, cathode on column side)
  - See `docs/button-matrix-circuit.txt` for full schematic
- **PL9823 LEDs (WS2812B-compatible) as 4pin 5mm LEDs:** data on pin 9, RGB color order
  - 3 LEDs per turnout (indicator, straight, turn)
  - 330Ω series resistor required on data line before first LED
  - 470–1000µF capacitor required across LED 5V/GND power rails
  - See `.github/instructions/fastled.instructions.md` for full details
- **Rotary encoder:** CLK on pin 2 (INT4), DT on pin 3 (INT5), SW on pin 8 (INPUT_PULLUP)
  - Used for setup mode navigation; long-press SW (≥3s) toggles setup mode
- **LCD:** TC1602A 16×2, 4-bit parallel (pins 46–51)
- **L298N Turnout motor driver (H-bridge):**
  - STRAIGHT = IN1 HIGH / IN2 LOW; TURN = IN1 LOW / IN2 HIGH; OFF = both LOW
  - **CRITICAL:** Motors must never be energized longer than `MOTOR_DURATION_MS` (500ms) — exceeding this damages the turnout mechanism

## Key Files

| File | Purpose |
|---|---|
| `src/main.cpp` | Application entry point — setup, loop, button callback, motor control |
| `include/pinLayout.h` | All hardware pin definitions and matrix dimensions |
| `docs/button-matrix-circuit.txt` | ASCII schematic of the 4×3 button matrix with diodes |
| `platformio.ini` | Build configuration and library dependencies |

## Libraries

| Library | Version | Purpose |
|---|---|---|
| FastLED | ^3.10.3 | PL9823 (WS2812B-compatible) LED control — see `.github/instructions/fastled.instructions.md` |
| ButtonMatrix (renerichter) | ^1.0.3 | Button matrix scanning with debounce, click/long-press detection, event callbacks |
| LiquidCrystal (arduino-libraries) | ^1.0.7 | TC1602A 16×2 LCD display |
| Encoder (paulstoffregen) | ^1.4.4 | Interrupt-driven rotary encoder reading |

## Architecture

### Turnout Struct

Each `Turnout` struct contains:
- `buttonIndex` — the button matrix number (0–11) that controls this turnout
- Motor control pins (`in1`, `in2`)
- Current state (`STRAIGHT` or `TURN`)
- LED indices for the 3 associated LEDs
- `configured` flag (unconfigured slots are ignored)
- Non-blocking motor timer (`motorActive`, `motorStartMs`)
- Click cooldown timer (`lastClickMs`)
- Display label (`name`)

The `turnouts[]` array is sized to `NUM_BUTTONS` (12). Only configured entries respond to presses. A turnout's array index does **not** need to match its `buttonIndex` — the lookup is handled by `findTurnoutByButton()`.

Turnouts are configured via the `configureTurnout()` helper which takes only the per-turnout fields and fills in shared defaults (`STRAIGHT`, `configured = true`, timers zeroed). If a new field is added to the `Turnout` struct with a default value, only `configureTurnout()` needs updating — not all 12 call sites.

### Button-to-Turnout Lookup

The `buttonIndex` field decouples the physical button from the array position. When a button is pressed, `onButtonAction` calls `findTurnoutByButton(buttonNum)` which linearly searches `turnouts[]` for a configured entry whose `buttonIndex` matches the pressed button's number. This allows turnouts to be stored at any array index regardless of which physical button controls them.

**Important:** Do NOT use `button.getNumber()` as a direct index into `turnouts[]`. Always go through `findTurnoutByButton()` to resolve the mapping.

### Button Matrix Initialization

The ButtonMatrix library is initialized with default (non-inverted) input logic, matching the hardware's idle-HIGH / pressed-LOW behavior:

1. `matrix.init()` — sets row pins to INPUT_PULLUP, column pins to INPUT
2. `matrix.setScanInterval(20)` — 20ms debounce/scan interval
3. `matrix.setMinLongPressDuration(0xFFFF)` — disables long press detection; this app only uses click events, and without this, holding a button > 2s would suppress the click on release
4. `matrix.registerButtonActionCallback(onButtonAction)` — register callback

**Do NOT use `setInvertInput()`** — the hardware's idle-HIGH (INPUT_PULLUP) / pressed-LOW behavior matches the library's default input logic (LOW = pressed). Using `setInvertInput()` causes all idle buttons to appear phantom-pressed, which triggers cascading bugs with the library's `isLongPressed()` mechanism.

### Stale `m_stateChanged` Flag (Critical)

The ButtonMatrix library's `Button::updateState()` sets `m_stateChanged = true` on any state transition but **never clears it internally**. The only way to clear it is by calling `hasStateChanged()`. If this flag is left stale, subsequent `update()` scans see `bChanged = true` for that button on every cycle, causing spurious click callbacks to fire continuously.

**Required:** After every `matrix.update()` call in `loop()`, drain the flags:
```cpp
for (uint8_t i = 0; i < NUM_BUTTONS; i++)
{
    buttons[i].hasStateChanged();
}
```

This is needed regardless of `setInvertInput()` — it is a workaround for a library bug where `updateState()` always returns the stale `m_stateChanged` value instead of only returning `true` on actual transitions.

### ButtonMatrix Constructor Parameter Order

```cpp
ButtonMatrix(Button* buttons, uint8_t* rowPins, uint8_t* colPins, uint8_t numRows, uint8_t numCols)
```

Row pins and numRows come **before** column pins and numCols. The code passes them as:
```cpp
ButtonMatrix matrix(buttons, rowPins, colPins, BTN_ROWS, BTN_COLS);
```

### Click Cooldown

A per-turnout cooldown (`CLICK_COOLDOWN_MS`, currently `MOTOR_DURATION_MS + 250` = 750ms) suppresses rapid repeated clicks. Each click during the cooldown window resets the timer, so the cooldown only expires after the full duration of inactivity.

### Motor Control

Motors are energized for `MOTOR_DURATION_MS` (500ms) using a non-blocking timer checked in `loop()`. While a motor is active, clicks on that turnout's button are ignored. **Motors must NEVER be energized longer than `MOTOR_DURATION_MS`** — the turnout mechanism can be damaged by prolonged power.

After the motor pulse completes in `loop()`, the turnout's state is saved to FRAM (in normal mode only). This ensures FRAM only records states the turnout physically reached.

### LED Feedback

Each turnout has 3 LEDs:
- **Indicator LED** — always green when configured
- **Straight LED** — green when STRAIGHT, red when TURN
- **Turn LED** — green when TURN, red when STRAIGHT

All LED updates go through `renderAllTurnoutLeds()` which clears the entire buffer and repaints every LED based on the canonical turnout state. LED fields set to `LED_UNASSIGNED` (0xFF) are skipped. This is called after FRAM load, config save, setup mode exit, and state toggles. See `.github/instructions/fastled.instructions.md` for full FastLED usage details.

### FRAM Persistence

Turnout state and LED indices are persisted across power cycles using an external **MB85RC256V FRAM module** (32 KB, I²C address `0x50`). The FRAM lives on a **private software-I²C bus** driven by `SoftwareWire` on pins **14 (SDA)** and **15 (SCL)** — the hardware TWI bus (pins 20/21) is reserved for the DCC-EX I²C slave role (`0x65`). Only 51 bytes are used.

**Layout:**
| Address | Content |
|---|---|
| 0 | Magic byte (0xA5) |
| 1 | Version byte (2) |
| 2–49 | 4 bytes per turnout × 12 (state, inLedIdx, straightLedIdx, turnLedIdx) |
| 50 | CRC8 over bytes 2–49 |

**Boot sequence:**
1. `configureTurnout()` calls set hardcoded defaults (motor pins, button mapping, names)
2. `fram.begin()` then `framProbe()` verifies the FRAM responds; sets the `framAvailable` flag. If probing fails, persistence is disabled for this power cycle but the system continues to operate.
3. `storageLoad()` checks magic + version + CRC; if valid, overrides state and LED indices
4. If invalid (first boot or corruption), `storageSaveAll()` writes current defaults
5. Motor initialization uses the loaded state (not always STRAIGHT)

**Write strategy:**
- `framWrite8()` is called unconditionally — FRAM has virtually unlimited write endurance (~10¹³ cycles), so there is no read-before-write optimization as there was with EEPROM.
- State saved after motor completion, not on button press
- LED config saved immediately when user confirms in setup mode
- All `storageSave*` / `storageLoad` calls short-circuit to a no-op when `!framAvailable`.

**Validation:** On load, each turnout's data is range-checked (`state <= TURN`, LED indices `< NUM_LEDS`). Invalid entries are skipped and retain hardcoded defaults.

### Setup Mode

A rotary encoder provides a setup interface for configuring turnout direction and LED indices without reflashing.

**Entering/exiting:** Long-press encoder switch (≥3 seconds). Unsaved changes are discarded on exit.

**State machine:**
```
SETUP_IDLE → (button press) → SETUP_DIRECTION → SETUP_IN_LED → SETUP_STRAIGHT_LED → SETUP_TURN_LED → (cycles back to DIRECTION)
```

**SETUP_DIRECTION:** On entry, the motor fires in the current "straight" direction so the user can observe the physical position. The LCD shows "Dir: STRAIGHT". The user rotates the encoder to relabel if the physical position doesn't match. No motor fires during rotation — only the label changes. Encoder rotation is blocked while the motor pulse is active.

**SETUP_*_LED fields:** The LCD shows the LED index. The LED at that index blinks white (250ms on/off). Rotating the encoder changes the index (wrapping 0–35).

**Saving:** Press the same button that started configuration. Config is written to FRAM immediately.

**Switching:** Press a different button to discard current changes and start configuring the new button.

**Encoder input:**
- CLK/DT: Paul Stoffregen's Encoder library (interrupt-driven, 4 counts per detent)
- SW: polled with 50ms debounce; long-press detection prevents also firing a short click

### LCD Display

The LCD shows a startup message with the count of configured turnouts, then reverts to an idle summary showing the count of STRAIGHT vs TURN turnouts. When a turnout is toggled, the LCD shows the turnout label and new state for `LCD_MESSAGE_MS` (3 seconds) before reverting to idle.

## Constraints & Gotchas

- **CRITICAL: Motor duration safety** — Motors must NEVER be energized longer than `MOTOR_DURATION_MS`. All motor activations must use the non-blocking timer pattern (`motorActive` + `motorStartMs` + shutoff in `loop()`), or the blocking `delay(MOTOR_DURATION_MS) + switchOff()` pattern in `setup()`. Failure to shut off motors will damage the turnout mechanism.
- **Do NOT use `setInvertInput()`** — see "Button Matrix Initialization" above for the full explanation.
- **Do NOT remove `setMinLongPressDuration(0xFFFF)`** — without it, the library's default 2-second long-press threshold will suppress clicks when a button is held too long.
- **Do NOT remove the `hasStateChanged()` drain loop** — see "Stale `m_stateChanged` Flag" above.
- Click events fire on button **release** (not press-down). This is the library's standard behavior with non-inverted input.
- Pin definitions belong in `include/pinLayout.h`, not in `main.cpp`.
- The project uses the `RSys` namespace from the ButtonMatrix library (`using namespace RSys;`).
- Serial baud rate is 9600.
- Turnout motor pin assignments in `pinLayout.h` are intentionally non-sequential (e.g., T5 = pins 34–35, T7 = pins 32–33) to match physical wiring.
- FRAM state is written after motor completion, not on button press — prevents persisting a state the turnout never physically reached.
- `FastLED.show()` disables all interrupts on AVR for ~1.1ms (36 LEDs at 800kHz). Encoder counts during very fast rotation could be missed. Acceptable for human-speed setup interaction.
- **Do NOT use pin 13 for LED data** — the on-board LED circuit degrades the signal. See `.github/instructions/fastled.instructions.md`.
