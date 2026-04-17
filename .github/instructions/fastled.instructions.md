# FastLED Library — Usage in Switching Controller

## LED Hardware

- **LED type:** PL9823 (WS2812B-compatible, 4-pin 5mm through-hole RGB)
- **Data pin:** 9 (`LED_PIN` in `pinLayout.h`)
- **Color order:** RGB
- **Count:** 36 LEDs (`NUM_LEDS` = 3 per turnout × 12 turnouts)
- **Chipset declaration:** `FastLED.addLeds<WS2812B, LED_PIN, RGB>(leds, NUM_LEDS)`

### Required External Components

- **330Ω series resistor** on the data line between Arduino pin 9 and the first LED's data-in. Dampens signal ringing that causes random bit corruption.
- **470–1000µF electrolytic capacitor** across the LED 5V and GND power rails, as close to the first LED as possible. Smooths voltage spikes during LED switching.

Without these components, the WS2812B data stream randomly corrupts, causing wrong colors (purple/blue artifacts), LEDs going dark, and different LEDs affected each time.

### Pin Selection

**Do NOT use pin 13** for LED data on the Arduino Mega 2560. Pin 13 has an on-board LED and resistor that add capacitance to the data line, degrading signal quality.

On the ATmega2560, pins map to different I/O ports with different access speeds:
- Pins on Port B (10–13, 50–53): 1-cycle `out` instruction (low I/O address)
- Pins on Port H (6–9): 2-cycle `sts` instruction (high I/O address)

FastLED's AVR clockless driver (`platforms/avr/clockless_trinket.h`) compensates for both via the `AVR_PIN_CYCLES` macro, so either port works correctly. Pin 9 (Port H, bit 6) is used in this project.

## How setBrightness() Works

`setBrightness(scale)` stores the brightness value in `m_Scale` (source: `FastLED.h`). It does **not** modify the LED buffer. When `show()` is called, it passes `m_Scale` to `showLedsInternal(scale)`, which applies scaling at output time via `scale8()` in the pixel controller pipeline.

This means:
- `setBrightness()` only needs to be called once in `setup()`
- The `leds[]` buffer always retains original full-intensity color values
- Reading back values from `leds[]` after `show()` returns the original colors, not scaled ones

This application calls `setBrightness(brightness)` before every `show()` as a defensive measure, though technically only the initial call in `setup()` is required.

## How show() Handles Interrupts on AVR

The WS2812B protocol has strict timing (250ns/625ns pulses per bit). On AVR, the clockless driver disables **all** interrupts during the data transmission:

```
cli()                      // Disable all interrupts
showRGBInternal(pixels)    // Bit-bang WS2812B protocol
// ... clock correction ...
sei()                      // Re-enable interrupts
```

This is controlled by `FASTLED_ALLOW_INTERRUPTS` (default: `0` = disabled). For 36 LEDs at 800kHz, the data send takes ~1.1ms. During this time:
- `millis()` loses at most 1 tick (Timer0 interrupt deferred then serviced)
- Encoder interrupts (INT4/INT5 on pins 2/3) are deferred but not lost
- Serial TX is buffered and unaffected

## Chipset Timing: WS2812B vs WS2811

From `chipsets.h`:

| Chipset | T0H | T1H | TLOW | Data Rate |
|---|---|---|---|---|
| WS2812B | 250ns | 625ns | 375ns | 800kHz |
| WS2811 (800kHz) | 320ns | 320ns | 640ns | 800kHz |
| WS2811 (400kHz) | 800ns | 800ns | 900ns | 400kHz |

**PL9823 LEDs use WS2812B timing.** Do NOT use `WS2811` — the different pulse widths cause random data corruption on PL9823 LEDs.

## Color Order

FastLED uses 3-bit octal encoding for byte order (from `fl/eorder.h`):

| Order | Byte 0 | Byte 1 | Byte 2 |
|---|---|---|---|
| RGB (0012) | Red | Green | Blue |
| GRB (0102) | Green | Red | Blue |

PL9823 LEDs use **RGB** order. If colors appear swapped (red shows where green should be), the color order parameter in `addLeds` is wrong.

## Usage Patterns in This Application

### Initialization
```cpp
FastLED.addLeds<WS2812B, LED_PIN, RGB>(leds, NUM_LEDS);
FastLED.setBrightness(brightness);  // brightness = 16
```

### Full Render (renderAllTurnoutLeds)
```cpp
fill_solid(leds, NUM_LEDS, CRGB::Black);  // Clear entire buffer
// ... set each LED based on turnout state ...
FastLED.setBrightness(brightness);
FastLED.show();
```

The full buffer is cleared and repainted on every render because `FastLED.show()` sends the entire `leds[]` array — there is no way to update individual LEDs. The clear prevents stale colors on LEDs whose indices were remapped via setup mode.

### Setup Mode Blink
During LED configuration, the selected LED blinks white/black at 250ms intervals. Only the single blink LED is modified between full renders.

### Colors Used
- `CRGB::Green` — indicator LED and active-direction LEDs
- `CRGB::Red` — inactive-direction LEDs
- `CRGB::White` — setup mode blink (on)
- `CRGB::Black` — off / buffer clear

### LED_UNASSIGNED (0xFF)
LED fields can be set to `0xFF` meaning "no LED assigned." All LED write paths guard against this value to prevent out-of-bounds array access. `renderAllTurnoutLeds()` skips any field set to `0xFF`.
