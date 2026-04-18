# DCC-EX Integration via I2C

## Problem Statement

Connect the Switching Controller (Arduino Mega 2560) to a DCC-EX EX-CSB1 command station over I2C so that DCC-EX can throw/close the 12 turnouts remotely, while preserving local button control. This requires:

1. A custom DCC-EX HAL driver (`IO_SwitchingController.h`) running on the EX-CSB1
2. I2C slave handler code added to the Switching Controller firmware on the Mega
3. A well-defined I2C communication protocol between the two
4. Physical I2C wiring with a logic level shifter (3.3V ↔ 5V)

## Approach

Model the integration after the existing DCC-EX **EX-IOExpander** architecture: a HAL driver on the command station side communicates over I2C with a slave Arduino. Our implementation is simpler since we only need digital write (throw/close) and digital read (current state + busy status) — no analogue, no servo profiles, no pin pullup configuration.

---

## 1. Circuit Connection Diagram

### Voltage Warning

The EX-CSB1 is ESP32-based and its I2C bus operates at **3.3V**. The Arduino Mega operates at **5V**. A **bidirectional I2C logic level shifter** is required. Connecting directly will damage the ESP32.

### Wiring Diagram

```
╔══════════════════════════════════════════════════════════════════════════════════╗
║             EX-CSB1 ↔ ARDUINO MEGA 2560 — I2C CONNECTION DIAGRAM               ║
╚══════════════════════════════════════════════════════════════════════════════════╝

                    3.3V SIDE                              5V SIDE
  ┌──────────────┐           ┌─────────────────┐           ┌──────────────────┐
  │   EX-CSB1    │           │  I2C LEVEL       │           │  ARDUINO MEGA    │
  │  (ESP32)     │           │  SHIFTER         │           │  2560            │
  │              │           │  (Bidirectional) │           │                  │
  │  I2C Header  │           │                  │           │                  │
  │  or QWiic    │           │                  │           │                  │
  │              │           │                  │           │                  │
  │  SDA ────────┼───────────┤ LV1        HV1 ├───────────┼──── Pin 20 (SDA) │
  │  SCL ────────┼───────────┤ LV2        HV2 ├───────────┼──── Pin 21 (SCL) │
  │  3.3V ───────┼───────────┤ LV         HV  ├───────────┼──── 5V           │
  │  GND ────────┼─────┬─────┤ GND        GND ├─────┬─────┼──── GND          │
  │              │     │     │                  │     │     │                  │
  └──────────────┘     │     └─────────────────┘     │     └──────────────────┘
                       │                              │
                       └──────────────────────────────┘
                              Common Ground
                       (BOTH boards must share GND)
```

### Required Components

| Component | Purpose | Notes |
|-----------|---------|-------|
| **Bidirectional I2C Level Shifter** | Convert 3.3V ↔ 5V I2C signals | BSS138-based modules (e.g., Adafruit #757, SparkFun BOB-12009) work well. Must support bidirectional open-drain signaling. |
| **4× jumper wires** | SDA, SCL, power, ground connections | Keep wires short (< 30cm / 12") to minimize capacitance |

### EX-CSB1 I2C Connection Points (choose one)

| Connector | Pins | Voltage | Notes |
|-----------|------|---------|-------|
| **QWiic** | SDA, SCL, 3.3V, GND | 3.3V always | Standard JST-SH 4-pin; check pin order on your cable |
| **Dual I2C Header** | SDA, SCL, V+, GND | 3.3V | 2×4 pin header; verify pin labels on board silkscreen |

### Arduino Mega I2C Pins

| Pin | Function | Notes |
|-----|----------|-------|
| 20 | SDA | Already reserved in `pinLayout.h` |
| 21 | SCL | Already reserved in `pinLayout.h` |

### I2C Pull-Up Resistors

The EX-CSB1 has on-board I2C pull-ups (controllable via solder jumper pads). Most level shifter modules include 10kΩ pull-ups on both sides. The Arduino Mega has internal pull-ups enabled by the Wire library. With a single device on the bus, this is typically fine. If you have multiple I2C devices, you may need to remove some pull-ups to keep the combined resistance above 1.7kΩ.

---

## 2. I2C Protocol Specification

### General Design

- **Master**: EX-CSB1 (DCC-EX CommandStation) via I2CManager
- **Slave**: Arduino Mega 2560 at configurable address (default **0x65**)
- **Bus speed**: 100kHz (standard mode — conservative for reliability with FastLED interrupt blocking)
- **Pin identity**: The `pin` byte in all commands refers to the **turnout array index** (0–11), NOT the button matrix number

### Protocol Commands

| Command | Code | Master Sends | Slave Responds | Description |
|---------|------|-------------|----------------|-------------|
| `SC_GETINFO` | `0xA0` | 1 byte: `[0xA0]` | 4 bytes: `[numConfigured, protocolVer, featureFlags, reserved]` | Get device capabilities |
| `SC_GETVER` | `0xA1` | 1 byte: `[0xA1]` | 3 bytes: `[major, minor, patch]` | Get firmware version |
| `SC_WRITE` | `0xA2` | 3 bytes: `[0xA2, pin, state]` | 1 byte: `[0xA9]` ACK or `[0xAF]` ERR | Set turnout to absolute state |
| `SC_READALL` | `0xA3` | 1 byte: `[0xA3]` | 4 bytes: `[stateLo, stateHi, busyLo, busyHi]` | Read all states + busy flags |

### Command Details

#### SC_GETINFO (0xA0)

Sent during HAL `_begin()` initialization. The slave responds with:

- `numConfigured`: Number of configured turnouts (0–12)
- `protocolVer`: Protocol version (currently 1)
- `featureFlags`: Reserved (0x00)
- `reserved`: Reserved (0x00)

If the slave is not ready (still in startup motor initialization), it responds with `numConfigured = 0`. The HAL driver retries until it gets a non-zero value.

#### SC_GETVER (0xA1)

Returns the firmware version of the Switching Controller. Informational only.

#### SC_WRITE (0xA2)

**Absolute-set semantics** (idempotent): `state=0` means STRAIGHT (DCC-EX "close"), `state=1` means TURN (DCC-EX "throw"). If the turnout is already in the requested state, this is a no-op. If the turnout motor is currently active (busy), the command is rejected with `0xAF` ERR.

The `pin` byte is the turnout array index (0–11). The HAL driver maps VPINs to pin indices: `pin = vpin - firstVpin`.

#### SC_READALL (0xA3)

Returns a 4-byte snapshot of all turnout states and motor-busy flags:

- `stateLo` / `stateHi`: 16-bit bitmask (bit N = turnout N state, 1=TURN, 0=STRAIGHT)
- `busyLo` / `busyHi`: 16-bit bitmask (bit N = 1 if turnout N motor is currently active)

The HAL driver uses the busy mask to report `isActive()` if needed, and the state mask to report `_read()` values. This allows DCC-EX to detect when a local button press changes a turnout state.

### Protocol Constants

```cpp
// Commands (Master → Slave)
#define SC_GETINFO  0xA0
#define SC_GETVER   0xA1
#define SC_WRITE    0xA2
#define SC_READALL  0xA3

// Responses
#define SC_RDY      0xA9   // Acknowledge / ready
#define SC_ERR      0xAF   // Error / rejected

// Default I2C address
#define SC_I2C_ADDRESS  0x65

// Protocol version
#define SC_PROTOCOL_VER 1
```

---

## 3. DCC-EX HAL Driver: `IO_SwitchingController.h`

A new file placed in the EX-CommandStation source directory alongside other `IO_*.h` files.

### Key Design Decisions

- Uses DCC-EX `I2CManager` for all I2C operations (not Arduino Wire)
- `_write()` uses blocking I2C (same pattern as EX-IOExpander)
- `_loop()` uses non-blocking I2C for periodic state polling
- Poll interval: **100ms** (conservative to avoid conflicts with FastLED interrupt blocking on the Mega)
- Retries on NACK/error with backoff
- **Automatic reconnection**: When device is `DEVSTATE_FAILED`, `_loop()` retries `SC_GETINFO` every 5 seconds. This allows the Mega to be connected after boot without requiring a DCC-EX restart

### Independent Operation & Hot-Plug

Both devices are designed to operate independently when the I2C bus is not connected:

- **Arduino Mega (slave):** `Wire.begin(address)` enables the TWI peripheral with internal pull-ups holding SDA/SCL at idle (HIGH). With no master present, no TWI interrupts fire, the I2C command queue stays empty, and `processI2CCommands()` is a no-op every loop iteration. All local functionality (buttons, LEDs, motors, LCD, EEPROM, setup mode) operates exactly as it does without any I2C code present.
- **EX-CSB1 (master/HAL driver):** During `_begin()`, if no slave responds at address 0x65, the driver sets `_deviceState = DEVSTATE_FAILED` and logs "OFFLINE". All `_write()` and `_read()` calls short-circuit. DCC-EX continues running trains, throttles, and all other HAL devices normally.
- **Connecting later:** When the I2C cable is plugged in while both are running, the HAL driver's periodic 5-second retry in `_loop()` will detect the Mega and transition to `DEVSTATE_NORMAL`. No reboot of either device is required. Note: I2C is not designed for hot-plugging — connect/disconnect while both are powered with care to avoid bus glitches.

### Skeleton

```cpp
#ifndef IO_SWITCHINGCONTROLLER_H
#define IO_SWITCHINGCONTROLLER_H

#include "IODevice.h"
#include "I2CManager.h"
#include "DIAG.h"

class SwitchingController : public IODevice {
public:
  static void create(VPIN firstVpin, int nPins, I2CAddress i2cAddress) {
    if (checkNoOverlap(firstVpin, nPins, i2cAddress))
      new SwitchingController(firstVpin, nPins, i2cAddress);
  }

private:
  SwitchingController(VPIN firstVpin, int nPins, I2CAddress i2cAddress) {
    _firstVpin = firstVpin;
    _nPins = min(nPins, 12);
    _I2CAddress = i2cAddress;
    addDevice(this);
  }

  void _begin() override {
    // Verify device presence, send SC_GETINFO, get capabilities
    // Retry up to 10 times with 500ms delay for startup
    // Send SC_GETVER to log firmware version
  }

  void _loop(unsigned long currentMicros) override {
    // If device is offline, retry SC_GETINFO every 5 seconds
    // On success, transition to DEVSTATE_NORMAL and resume polling
    //
    // Non-blocking poll: send SC_READALL every 100ms
    // On response, update _statesMask and _busyMask
    // Compare with previous states to detect local changes
  }

  void _write(VPIN vpin, int value) override {
    // Blocking I2C: send SC_WRITE [pin, state]
    // pin = vpin - _firstVpin
    // value: 0 = STRAIGHT (close), 1 = TURN (throw)
    // Update cached state on success
  }

  int _read(VPIN vpin) override {
    // Return cached state bit from last _loop() poll
    int pin = vpin - _firstVpin;
    return (_statesMask >> pin) & 1;
  }

  void _display() override {
    DIAG(F("SwitchCtrl I2C:%s v%d.%d.%d Vpins %u-%u %S"),
         _I2CAddress.toString(), _majorVer, _minorVer, _patchVer,
         (int)_firstVpin, (int)_firstVpin + _nPins - 1,
         _deviceState == DEVSTATE_FAILED ? F("OFFLINE") : F(""));
  }

  // Cached state from polling
  uint16_t _statesMask = 0;
  uint16_t _busyMask = 0;
  uint8_t _numConfigured = 0;
  uint8_t _majorVer = 0, _minorVer = 0, _patchVer = 0;

  // Non-blocking I2C
  I2CRB _i2crb;
  uint8_t _readCommandBuffer[1];
  uint8_t _readResponseBuffer[4];
  unsigned long _lastPollMicros = 0;
  static const unsigned long _pollIntervalUs = 100000UL; // 100ms
  static const unsigned long _retryIntervalUs = 5000000UL; // 5s reconnect retry
  bool _pollPending = false;
};

#endif
```

### DCC-EX Configuration

In `myHal.cpp` on the EX-CSB1:

```cpp
#include "IO_SwitchingController.h"

void halSetup() {
  // 12 VPINs starting at 800, I2C address 0x65
  SwitchingController::create(800, 12, 0x65);
}
```

In `mySetup.h` or via serial commands, define the turnouts:

```
SETUP("<T 1 VPIN 800>");    // T01 → VPIN 800 → turnout index 0
SETUP("<T 2 VPIN 801>");    // T02 → VPIN 801 → turnout index 1
SETUP("<T 3 VPIN 802>");    // T03 → VPIN 802 → turnout index 2
SETUP("<T 4 VPIN 803>");    // T04 → VPIN 803 → turnout index 3
SETUP("<T 5 VPIN 804>");    // T05 → VPIN 804 → turnout index 4
SETUP("<T 6 VPIN 805>");    // T06 → VPIN 805 → turnout index 5
SETUP("<T 7 VPIN 806>");    // T07 → VPIN 806 → turnout index 6
SETUP("<T 8 VPIN 807>");    // T08 → VPIN 807 → turnout index 7
SETUP("<T 9 VPIN 808>");    // T09 → VPIN 808 → turnout index 8
SETUP("<T 10 VPIN 809>");   // T10 → VPIN 809 → turnout index 9
SETUP("<T 11 VPIN 810>");   // T11 → VPIN 810 → turnout index 10
SETUP("<T 12 VPIN 811>");   // T12 → VPIN 811 → turnout index 11
```

Then from any DCC-EX throttle or JMRI:

```
<T 1 T>     // Throw turnout T01
<T 1 C>     // Close turnout T01
<JT>        // List all turnout states
```

---

## 4. Arduino Mega I2C Slave Code Changes

### New Files

| File | Purpose |
|------|---------|
| `include/i2c_protocol.h` | Shared protocol constants (command codes, response codes, address) |
| `include/i2c_slave.h` | I2C slave handler declarations |
| `src/i2c_slave.cpp` | I2C slave handler implementation |

### Changes to Existing Files

| File | Change |
|------|--------|
| `include/pinLayout.h` | Add `I2C_SDA` (20), `I2C_SCL` (21), `I2C_ADDRESS` (0x65) defines |
| `src/main.cpp` | Include `i2c_slave.h`, call `i2cSlaveSetup()` after motor init, call `processI2CCommands()` and `updateI2CStateSnapshot()` in `loop()` |

### I2C Slave Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    ISR CONTEXT (TWI interrupt)                │
│                                                              │
│  receiveEvent(numBytes)          requestEvent()              │
│  ├─ Parse command byte           ├─ Check outboundFlag       │
│  ├─ For SC_WRITE: push to        ├─ Write pre-built response │
│  │   ring buffer                 └─ (responseBuffer[])       │
│  ├─ For SC_READALL: set flag                                 │
│  └─ For SC_GETINFO/VER: set flag                             │
│                                                              │
│  ⚠ NO motor control, EEPROM, FastLED, or Serial here!       │
└──────────────────────────┬──────────────────────────────────┘
                           │ volatile flags + ring buffer
                           ▼
┌─────────────────────────────────────────────────────────────┐
│                    MAIN LOOP CONTEXT                         │
│                                                              │
│  processI2CCommands()                                        │
│  ├─ Dequeue SC_WRITE commands from ring buffer               │
│  ├─ For each: validate turnout index                         │
│  ├─ If state != requested AND motor not busy:                │
│  │   ├─ Drive motor (driveStraight/driveTurn)                │
│  │   ├─ Update turnout state                                 │
│  │   ├─ Start motor timer (motorActivate)                    │
│  │   ├─ Update LEDs (renderAllTurnoutLeds)                   │
│  │   ├─ Update LCD                                           │
│  │   └─ Mark pendingEepromSave                               │
│  └─ If state already matches: no-op (idempotent)             │
│                                                              │
│  updateI2CStateSnapshot()                                    │
│  ├─ Build 16-bit statesMask from turnouts[].state            │
│  ├─ Build 16-bit busyMask from turnouts[].motorActive        │
│  └─ Write to volatile response buffer (atomic via            │
│     noInterrupts)                                            │
└─────────────────────────────────────────────────────────────┘
```

### Ring Buffer for Write Commands

```cpp
struct I2CWriteCmd {
  uint8_t pin;    // Turnout index (0-11)
  uint8_t state;  // 0=STRAIGHT, 1=TURN
};

#define I2C_CMD_QUEUE_SIZE 8
volatile I2CWriteCmd i2cCmdQueue[I2C_CMD_QUEUE_SIZE];
volatile uint8_t i2cCmdHead = 0;
volatile uint8_t i2cCmdTail = 0;
```

### Startup Sequencing

The existing `setup()` function drives all 12 motors sequentially (~6 seconds). The I2C slave must NOT be initialized until this completes, or the HAL driver will get stale/incorrect responses.

```
setup() {
  // ... existing code: FastLED, configureTurnout, eepromLoad, motor init ...

  // Initialize I2C slave AFTER all motors are positioned
  i2cSlaveSetup();   // Wire.begin(I2C_ADDRESS) + register callbacks
  i2cReady = true;   // SC_GETINFO will now report numConfigured > 0

  // ... existing code: matrix.init, encoder, LCD ...
}
```

### Integration with Existing Toggle Logic

The `processI2CCommands()` function reuses the same motor control functions as `onButtonAction()`:

```cpp
void processI2CCommands() {
  while (i2cCmdHead != i2cCmdTail) {
    I2CWriteCmd cmd;
    noInterrupts();
    cmd = i2cCmdQueue[i2cCmdTail];
    i2cCmdTail = (i2cCmdTail + 1) % I2C_CMD_QUEUE_SIZE;
    interrupts();

    if (cmd.pin >= NUM_BUTTONS || !turnouts[cmd.pin].configured)
      continue;

    Turnout &t = turnouts[cmd.pin];

    // Idempotent: skip if already in requested state
    if ((cmd.state == 0 && t.state == STRAIGHT) ||
        (cmd.state == 1 && t.state == TURN))
      continue;

    // Skip if motor is busy
    if (t.motorActive)
      continue;

    // Drive motor to requested state
    if (cmd.state == 1) {
      driveTurn(t);
      t.state = TURN;
    } else {
      driveStraight(t);
      t.state = STRAIGHT;
    }

    renderAllTurnoutLeds();
    t.pendingEepromSave = (appMode == MODE_NORMAL);
    motorActivate(t);

    // LCD feedback
    // ... show "DCC: T01 -> TURN" ...
  }
}
```

---

## 5. FastLED + I2C Timing Considerations

### The Problem

`FastLED.show()` disables ALL interrupts for ~1.1ms (36 LEDs at 800kHz). During this window, the TWI slave interrupt is deferred. If the EX-CSB1 sends an I2C transaction during this window, the Mega cannot ACK in time.

### Mitigations

1. **100ms poll interval** — Reduces collision probability (~1.1% chance per poll)
2. **100kHz I2C bus speed** — More tolerant of delays than 400kHz
3. **HAL driver retry logic** — On NACK or error, retry up to 3 times with 10ms backoff
4. **ESP32 I2CManager timeout** — The DCC-EX I2CManager handles clock stretching and has configurable timeouts
5. **Acceptable degradation** — A single missed poll every few seconds is imperceptible for turnout state feedback

---

## 6. VPIN-to-Turnout Mapping Reference

| DCC-EX Turnout ID | VPIN | Turnout Array Index | Turnout Label | Motor Pins | Button Index |
|--------------------|------|---------------------|---------------|------------|--------------|
| 1 | 800 | 0 | T01 | 22, 23 | 5 |
| 2 | 801 | 1 | T02 | 24, 25 | 0 |
| 3 | 802 | 2 | T03 | 26, 27 | 4 |
| 4 | 803 | 3 | T04 | 28, 29 | 1 |
| 5 | 804 | 4 | T05 | 30, 31 | 6 |
| 6 | 805 | 5 | T06 | 34, 35 | 3 |
| 7 | 806 | 6 | T07 | 36, 37 | 2 |
| 8 | 807 | 7 | T08 | 32, 33 | 9 |
| 9 | 808 | 8 | T09 | 42, 43 | 10 |
| 10 | 809 | 9 | T10 | 38, 39 | 8 |
| 11 | 810 | 10 | T11 | 40, 41 | 11 |
| 12 | 811 | 11 | T12 | 44, 45 | 7 |

> **Note:** The `pin` byte in the I2C protocol refers to the **turnout array index** column (0–11), NOT the button index. The HAL driver computes this as `pin = vpin - firstVpin`.

---

## 7. Implementation Todos

### Todo 1: Create shared protocol header (`i2c-protocol`)

Create `include/i2c_protocol.h` with command codes, response codes, protocol version, default I2C address, and shared data structures.

### Todo 2: Implement I2C slave handler (`i2c-slave`)

Create `include/i2c_slave.h` and `src/i2c_slave.cpp` with:

- Ring buffer for write commands
- `receiveEvent()` / `requestEvent()` ISR callbacks
- `i2cSlaveSetup()` initialization function
- `processI2CCommands()` main-loop processor
- `updateI2CStateSnapshot()` state builder
- Firmware version constant

**Depends on:** `i2c-protocol`

### Todo 3: Integrate I2C slave into main.cpp (`integrate-main`)

- Include `i2c_slave.h`
- Call `i2cSlaveSetup()` after motor initialization in `setup()`
- Call `processI2CCommands()` and `updateI2CStateSnapshot()` in `loop()`
- Add I2C address define to `pinLayout.h`

**Depends on:** `i2c-slave`

### Todo 4: Create DCC-EX HAL driver (`hal-driver`)

Create `IO_SwitchingController.h` for the EX-CommandStation:

- IODevice subclass with `create()`, `_begin()`, `_loop()`, `_write()`, `_read()`, `_display()`
- Non-blocking polling in `_loop()`
- Blocking write in `_write()`
- Retry logic with backoff
- Documentation header with usage examples

**Depends on:** `i2c-protocol`

### Todo 5: Create example DCC-EX configuration files (`dcc-ex-config`)

Create example `myHal.cpp` and `mySetup.h` files showing how to configure the HAL driver and define the 12 turnouts.

**Depends on:** `hal-driver`

### Todo 6: Create integration documentation (`docs`)

- Update circuit diagram docs
- I2C protocol reference
- Setup instructions for both sides
- Troubleshooting guide

**Depends on:** all above

### Todo 7: Test the integration (`testing`)

- Unit test I2C protocol parsing on Mega side
- Integration test with DCC-EX serial commands
- Verify bidirectional state sync (local button + DCC-EX command)
- Verify motor safety (no over-energization from I2C commands)
- Verify FastLED timing doesn't cause persistent I2C failures

**Depends on:** `integrate-main`, `hal-driver`

---

## 8. Troubleshooting

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| DCC-EX shows "OFFLINE" for SwitchCtrl | Device not found on I2C bus | Check wiring, level shifter, I2C address, verify Mega is powered and booted |
| Turnouts don't respond to DCC-EX commands | SC_WRITE rejected or not reaching Mega | Check serial monitor on both sides; verify VPIN mapping matches turnout indices |
| Turnout states don't update in DCC-EX after local button press | Poll not returning updated states | Verify `updateI2CStateSnapshot()` is called in Mega `loop()`; check I2C error rate in DCC-EX diagnostics |
| Random I2C errors / NACK | FastLED interrupt blocking | Reduce LED count, increase poll interval, verify retry logic is active |
| Wrong turnout throws when commanded | Pin identity mismatch | Verify DCC-EX VPIN-to-turnout-index mapping matches table in Section 6 |
| ESP32 damage / board failure | Missing level shifter | **Always** use a bidirectional I2C level shifter between the 3.3V EX-CSB1 and 5V Mega |
| Turnout motor runs indefinitely | Bug in I2C command processing | Verify `processI2CCommands()` uses `motorActivate()` and the existing non-blocking motor timer in `loop()` shuts it off after `MOTOR_DURATION_MS` |
