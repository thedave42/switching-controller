# Plan: Migrate from EEPROM to FRAM (MB85RC256V)

## Problem

The application currently uses the ATmega2560's built-in EEPROM for persistent storage. EEPROM has limited write endurance (~100K cycles per cell). Migrating to an external FRAM module (MB85RC256V) provides virtually unlimited write endurance (~10 trillion cycles) and 32KB of storage.

## Hardware — FRAM Wiring

The HiLetgo MB85RC256V breakout communicates over I2C. On the Mega 2560, the I2C bus is on pins 20 (SDA) and 21 (SCL) — both currently unoccupied.

| FRAM Board Pin | Arduino Mega Pin | Notes |
|---|---|---|
| VCC | 5V | Board supports 3.3V–5V |
| GND | GND | |
| SDA | 20 | Mega I2C data |
| SCL | 21 | Mega I2C clock |
| WP | GND | Write protect disabled |
| A0, A1, A2 | GND | Default I2C address 0x50 |

## Library

Adafruit_FRAM_I2C — drop-in Arduino library for the MB85RC256V.

API: `fram.begin()`, `fram.read8(addr)`, `fram.write8(addr, val)`.

PlatformIO: `adafruit/Adafruit FRAM I2C`

## Approach

The data layout (magic, version, CRC, 4 bytes per turnout) stays identical — only the storage backend changes. All function and constant names are renamed from `eeprom*`/`EEPROM_*` to `storage*`/`STORAGE_*` since the backend is no longer EEPROM.

### API Mapping

| Current (EEPROM) | New (FRAM) |
|---|---|
| `#include <EEPROM.h>` | `#include <Adafruit_FRAM_I2C.h>` |
| `EEPROM.read(addr)` | `fram.read8(addr)` |
| `EEPROM.update(addr, val)` | `fram.write8(addr, val)` |
| Implicit init | `fram.begin()` in `setup()` with error handling |

Note: `EEPROM.update()` skips writes when the value is unchanged (wear optimization). FRAM has virtually unlimited write endurance, so `write8()` can be called unconditionally. We still use `write8()` directly — no read-before-write optimization needed.

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

### First Boot

On first boot with a new FRAM, all memory reads as 0xFF. `storageLoad()` will fail the magic byte check and write defaults — this is the existing first-boot behavior, no special migration needed.

## File Changes

### `platformio.ini`
- Add `adafruit/Adafruit FRAM I2C` to lib_deps

### `src/main.cpp`
- Remove `#include <EEPROM.h>` — no longer used
- Add `#include <Adafruit_FRAM_I2C.h>`
- Add global `Adafruit_FRAM_I2C fram;`
- Add `fram.begin()` in `setup()` before `storageLoad()`, with serial error if FRAM not found
- Replace all `EEPROM.read()` → `fram.read8()`
- Replace all `EEPROM.update()` → `fram.write8()`
- Rename all `eeprom*` functions and `EEPROM_*` constants to `storage*`/`STORAGE_*`
- Update all call sites to use new names

### `include/pinLayout.h`
- No changes (I2C pins are handled by the Wire library internally)

### `.github/copilot-instructions.md`
- Update storage section: FRAM replaces EEPROM, document wiring, library, and I2C address

## What Stays The Same

- Data layout (magic byte, version, CRC, 4 bytes per turnout)
- All storage addresses
- CRC8 function logic (just reads from FRAM instead of EEPROM)
- All validation logic
- Motor safety ISR, LED conflict resolution, setup mode — completely unaffected

## Todos

1. Add Adafruit FRAM I2C library to platformio.ini
2. Replace EEPROM include with FRAM include, add global fram object
3. Add fram.begin() to setup() with error handling
4. Rename all EEPROM_* constants to STORAGE_*
5. Rename all eeprom* functions to storage*
6. Replace all EEPROM.read()/EEPROM.update() calls with fram.read8()/fram.write8()
7. Update all call sites to use new function names
8. Update copilot-instructions.md
