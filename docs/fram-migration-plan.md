# Plan: Migrate from EEPROM to FRAM (MB85RC256V) over Software I2C

## Problem

The application currently uses the ATmega2560's built-in EEPROM for persistent storage. EEPROM has limited write endurance (~100K cycles per cell). Migrating to an external FRAM module (MB85RC256V) provides virtually unlimited write endurance (~10 trillion cycles) and 32KB of storage.

## Constraint: Hardware I2C bus is reserved for DCC-EX

The Mega's hardware I2C peripheral (TWI on pins 20/21, the `Wire` library) is now operating as an **I2C slave** at address `0x65`, used by the DCC-EX EX-CSB1 to drive the turnouts (`src/i2c_slave.cpp`). The AVR TWI peripheral cannot operate as both master and slave simultaneously, so the FRAM cannot share this bus.

**Solution:** Place the FRAM on a **second, software-bit-banged I2C bus** on free GPIO pins. The Mega is master on this private bus; only the FRAM is on it. This keeps the hardware I2C dedicated to DCC-EX and isolates FRAM operations from any DCC-EX traffic.

Software I2C master is well-established for AVR — the `SoftwareWire` (Testato) library implements an Arduino-compatible API on arbitrary digital pins. Software I2C *slave* is fragile and would not be appropriate, but software I2C *master* (which is what we need for FRAM) is reliable because we control the clock and the timing of every transaction.

## Hardware — FRAM Wiring

The FRAM lives on its **own software I2C bus**, NOT on the Mega's hardware TWI pins (20/21).

### Pin Assignment

Pins 14 and 15 on the Mega are currently free (per `include/pinLayout.h` and the project pin map). They will become the SoftwareWire SDA/SCL.

| FRAM Board Pin | Arduino Mega Pin | Notes |
|---|---|---|
| VCC | 5V | Board supports 3.3V–5V |
| GND | GND | |
| SDA | 14 (`FRAM_SDA`) | Software I2C data — NOT shared with DCC-EX bus |
| SCL | 15 (`FRAM_SCL`) | Software I2C clock — NOT shared with DCC-EX bus |
| WP | GND | Write protect disabled |
| A0, A1, A2 | GND | Default I2C address `0x50` |

### Pull-up Resistors

The FRAM module sits alone on this private bus, so external pull-ups are required (the Mega's internal pull-ups are too weak for reliable I2C). Use 4.7kΩ from each of SDA and SCL to 5V. Most MB85RC256V breakout boards (HiLetgo, Adafruit) include these pull-ups already — verify on the schematic before adding more.

### Wiring Diagram

```
   ┌──────────────────┐                     ┌─────────────────┐
   │  ARDUINO MEGA    │                     │  MB85RC256V     │
   │  2560            │                     │  FRAM Module    │
   │                  │                     │                 │
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

If breakout has no on-board pull-ups, add:
   SDA ──[4.7kΩ]── 5V
   SCL ──[4.7kΩ]── 5V
```

## Library

**`SoftwareWire`** by Testato. It exposes the same `beginTransmission()` / `write()` / `endTransmission()` / `requestFrom()` / `read()` API as the standard `Wire` library, but on user-specified pins.

PlatformIO: `Testato/SoftwareWire` (or by URL: `https://github.com/Testato/SoftwareWire`)

The Adafruit FRAM I2C library cannot be used because it accepts only `TwoWire*`, not `SoftwareWire*`. Instead we will wrap a small set of FRAM access helpers (`framRead8`, `framWrite8`) directly against `SoftwareWire`. The MB85RC256V protocol is trivial:

```cpp
// Write byte: START | addr<<1|W | memHi | memLo | data | STOP
// Read byte:  START | addr<<1|W | memHi | memLo | RESTART | addr<<1|R | data | STOP
```

These two functions are short enough that no third-party FRAM library is needed.

## Approach

The data layout (magic, version, CRC, 4 bytes per turnout) stays identical — only the storage backend changes. All function and constant names are renamed from `eeprom*`/`EEPROM_*` to `storage*`/`STORAGE_*` since the backend is no longer EEPROM.

### API Mapping

| Current (EEPROM) | New (FRAM via SoftwareWire) |
|---|---|
| `#include <EEPROM.h>` | `#include <SoftwareWire.h>` |
| Implicit init | `SoftwareWire fram(FRAM_SDA, FRAM_SCL);` global; `fram.begin();` in `setup()` |
| `EEPROM.read(addr)` | `framRead8(addr)` (helper using `SoftwareWire`) |
| `EEPROM.update(addr, val)` | `framWrite8(addr, val)` (helper using `SoftwareWire`) |

Note: `EEPROM.update()` skips writes when the value is unchanged (wear optimization). FRAM has virtually unlimited write endurance, so `framWrite8()` can be called unconditionally — no read-before-write needed.

### Renames

| Current | New |
|---|---|
| `EEPROM_MAGIC` | `STORAGE_MAGIC` |
| `EEPROM_VERSION` | `STORAGE_VERSION` |
| `EEPROM_ADDR_MAGIC` | `STORAGE_ADDR_MAGIC` |
| `EEPROM_ADDR_VERSION` | `STORAGE_ADDR_VERSION` |
| `EEPROM_ADDR_DATA` | `STORAGE_ADDR_DATA` |
| `EEPROM_BYTES_PER_TURNOUT` | `STORAGE_BYTES_PER_TURNOUT` |
| `EEPROM_ADDR_CRC` | `STORAGE_ADDR_CRC` |
| `eepromCRC8()` | `storageCRC8()` |
| `eepromSaveTurnout()` | `storageSaveTurnout()` |
| `eepromSaveAll()` | `storageSaveAll()` |
| `eepromLoad()` | `storageLoad()` |
| `Turnout::pendingEepromSave` | `Turnout::pendingStorageSave` |
| Serial debug prefix `"EEPROM: ..."` | `"FRAM: ..."` |

`STORAGE_VERSION` keeps the current value (`2`) — the data layout is unchanged, so existing version compatibility logic stays intact.

### First Boot

On first boot with a new FRAM, all memory typically reads as `0xFF` (or random). `storageLoad()` will fail the magic byte check and `setup()` falls through to `storageSaveAll()` to write defaults. This is identical to the existing first-boot behavior — no special migration needed.

### `fram.begin()` Failure Handling

If FRAM initialization fails (no module attached, wiring fault), `setup()` should print a serial warning and continue running with in-memory defaults. `storageLoad()` will fail (returning false) and the system will operate normally for the current power cycle but won't persist state. This mirrors the behavior on a corrupt EEPROM today. Specifically:

- Add a `bool framAvailable` global, set after `fram.begin()` succeeds and a sanity write-then-read of a probe byte succeeds.
- `storageLoad()` short-circuits to `return false` when `!framAvailable`.
- `storageSaveTurnout()` / `storageSaveAll()` early-return when `!framAvailable`.

## File Changes

### `platformio.ini`
- Add `Testato/SoftwareWire` to `lib_deps`.
- Remove no library — `EEPROM.h` is part of the Arduino core, not a `lib_deps` entry.

### `include/pinLayout.h`
- Add `#define FRAM_SDA 14` and `#define FRAM_SCL 15`.
- Update the comment block at the top to reflect that pins 14 and 15 are now in use for software I2C to FRAM.

### `src/main.cpp`
- Remove `#include <EEPROM.h>`.
- Add `#include <SoftwareWire.h>`.
- Add global `SoftwareWire fram(FRAM_SDA, FRAM_SCL);` and `bool framAvailable = false;`.
- Add `framRead8(uint16_t addr)` and `framWrite8(uint16_t addr, uint8_t val)` helpers (use `uint16_t` for memory address even though we only use 0–50, because MB85RC256V requires a 16-bit address word in the protocol).
- In `setup()`, call `fram.begin();` before `storageLoad()`. Probe the FRAM (write a byte to a scratch address then read it back) to set `framAvailable`. Print serial status.
- Replace all `EEPROM.read(...)` → `framRead8(...)`.
- Replace all `EEPROM.update(...)` → `framWrite8(...)`.
- Rename all `eeprom*` functions and `EEPROM_*` constants per the table above.
- Update all call sites (line 776, 805, 808, 477, 570, 606, 953, 955).
- Update serial debug strings from `"EEPROM: ..."` to `"FRAM: ..."`.

### `src/turnout_types.h` (or wherever `Turnout` is defined)
- Rename field `pendingEepromSave` → `pendingStorageSave`.

### `src/i2c_slave.cpp`
- No changes. The hardware `Wire` slave behavior is unaffected.

### `.github/copilot-instructions.md`
- Update the EEPROM Persistence section: rename to "FRAM Persistence", document FRAM module, SoftwareWire pins (14/15), MB85RC256V address (0x50), and that the hardware I2C bus is reserved for DCC-EX.

## What Stays The Same

- Data layout (magic byte, version, CRC, 4 bytes per turnout) — `STORAGE_VERSION` remains `2`.
- All storage addresses (0–50).
- CRC8 function logic (now reads from FRAM instead of EEPROM).
- All validation logic (range checks on state and LED indices).
- Motor safety, LED conflict resolution, setup mode, button matrix handling, encoder, LCD, DCC-EX I2C slave — all unaffected.

## Out of Scope

- Motor shutoff path remains polled in `loop()` (no Timer-ISR migration). The existing EEPROM code already blocks ~16 ms per save; SoftwareWire FRAM is faster (~3 ms per save) and represents a net safety improvement, well within mechanical tolerance.
- No expansion of the persistence schema — only the backend changes.

## Todos

1. Add `Testato/SoftwareWire` to `platformio.ini`.
2. Add `FRAM_SDA` (14) and `FRAM_SCL` (15) defines to `include/pinLayout.h` and update the pin comment block.
3. Replace `<EEPROM.h>` include with `<SoftwareWire.h>`; add global `SoftwareWire fram(...)` and `bool framAvailable`.
4. Implement `framRead8()` / `framWrite8()` helpers against the `SoftwareWire` API (handles MB85RC256V 16-bit address word).
5. Add `fram.begin()` + probe write/read in `setup()` with serial status output; gate all storage ops on `framAvailable`.
6. Rename all `EEPROM_*` constants to `STORAGE_*`.
7. Rename all `eeprom*` functions to `storage*`.
8. Replace all `EEPROM.read()` / `EEPROM.update()` calls with `framRead8()` / `framWrite8()`.
9. Rename `Turnout::pendingEepromSave` field to `pendingStorageSave` and update its three call sites.
10. Update serial debug string prefixes from `"EEPROM: ..."` to `"FRAM: ..."`.
11. Update `.github/copilot-instructions.md` storage section.
12. Build and smoke-test: cold boot writes defaults, warm boot loads stored state, button toggle persists across power cycle, DCC-EX I2C still works concurrently with FRAM writes.
