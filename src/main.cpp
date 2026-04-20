#include <Arduino.h>
#include <SoftwareWire.h>
#include <Encoder.h>
#include <FastLED.h>
#include <LiquidCrystal.h>
#include "ButtonMatrix.h"

#include "pinLayout.h"
#include "turnout_types.h"
#include "i2c_slave.h"

using namespace RSys;

// --- Constants ---
const unsigned long MOTOR_DURATION_MS = 500;                     // How long to energize the turnout motor (ms)
const unsigned long CLICK_COOLDOWN_MS = MOTOR_DURATION_MS + 250; // Ignore repeated clicks within this window (ms)
const int brightness = 16; // 0-255
const unsigned long LCD_MESSAGE_MS = 3000;// How long to show action on LCD before reverting to idle
const unsigned long ENC_LONG_PRESS_MS = 3000; // Encoder switch long-press threshold
const unsigned long ENC_DEBOUNCE_MS = 50;     // Encoder switch debounce
const unsigned long BLINK_INTERVAL_MS = 250;  // LED blink rate during setup
const uint8_t LED_UNASSIGNED = 0xFF;          // Sentinel for "no LED assigned"

// --- Storage layout (FRAM MB85RC256V via SoftwareWire) ---
const uint8_t STORAGE_MAGIC = 0xA5;
const uint8_t STORAGE_VERSION = 2;
const uint8_t STORAGE_ADDR_MAGIC = 0;
const uint8_t STORAGE_ADDR_VERSION = 1;
const uint8_t STORAGE_ADDR_DATA = 2;          // 4 bytes per turnout × 12 = 48 bytes
const uint8_t STORAGE_BYTES_PER_TURNOUT = 4;  // state, inLedIdx, straightLedIdx, turnLedIdx
const uint8_t STORAGE_ADDR_CRC = STORAGE_ADDR_DATA + (STORAGE_BYTES_PER_TURNOUT * 12); // byte 50

// --- FRAM (MB85RC256V) on a private software I2C bus ---
// The hardware Wire bus is reserved for the DCC-EX I2C slave role (addr 0x65),
// so the FRAM sits on its own bit-banged bus via SoftwareWire on pins 14/15.
const uint8_t FRAM_I2C_ADDR = 0x50;           // A0=A1=A2 tied to GND
SoftwareWire fram(FRAM_SDA, FRAM_SCL);
bool framAvailable = false;

// --- LED array ---
CRGB leds[NUM_LEDS];

Turnout turnouts[NUM_BUTTONS]; // Array to hold the turnout configurations for each button

// --- Button Matrix ---
uint8_t colPins[BTN_COLS] = {BTN_COL0, BTN_COL1, BTN_COL2};
uint8_t rowPins[BTN_ROWS] = {BTN_ROW0, BTN_ROW1, BTN_ROW2, BTN_ROW3};

Button buttons[NUM_BUTTONS] = {
    Button(0), Button(1), Button(2), Button(3),
    Button(4), Button(5), Button(6), Button(7),
    Button(8), Button(9), Button(10), Button(11)};

ButtonMatrix matrix(buttons, rowPins, colPins, BTN_ROWS, BTN_COLS);

// --- LCD ---
LiquidCrystal lcd(LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
unsigned long lcdMessageMs = 0;
bool lcdShowingAction = false;

// --- Rotary encoder ---
Encoder encoder(ENC_CLK, ENC_DT);
long encLastPos = 0;
bool encSwState = HIGH;
bool encSwLastState = HIGH;
unsigned long encSwDebounceMs = 0;
unsigned long encSwPressStartMs = 0;
bool encSwPressed = false;  // Currently held down (debounced)
bool encLongPressHandled = false; // Prevent long-press from also firing short click

// --- App mode and setup state ---
AppMode appMode = MODE_NORMAL;
SetupField setupField = SETUP_IDLE;
int8_t setupTurnoutIdx = -1;   // Index into turnouts[] being configured
uint8_t setupButtonNum = 0xFF; // Button number that selected the turnout

// Temporary values during setup (not committed until save)
SwitchState setupDirection;
uint8_t setupInLed;
uint8_t setupStraightLed;
uint8_t setupTurnLed;

// LED blink state during setup
unsigned long blinkLastToggleMs = 0;
bool blinkOn = false;

// --- Forward declarations ---
Turnout *findTurnoutByButton(uint8_t buttonNum);
void configureTurnout(uint8_t index, uint8_t buttonIndex, uint8_t in1, uint8_t in2,
                      uint8_t inLed, uint8_t straightLed, uint8_t turnLed, bool reversed, const char *name);
void onButtonAction(Button &button);
void switchOff(int, int);
void switchStraight(int, int);
void switchTurn(int, int);
void driveStraight(Turnout &t);
void driveTurn(Turnout &t);
void motorActivate(Turnout &t);
void lcdShowIdle();
const char *getTurnoutLabel(uint8_t index, char *buf, uint8_t bufSize);
void renderAllTurnoutLeds();
uint8_t framRead8(uint16_t addr);
uint8_t framWrite8(uint16_t addr, uint8_t val);
bool framProbe();
uint8_t storageCRC8(uint8_t startAddr, uint8_t length);
void storageSaveTurnout(uint8_t index);
void storageSaveAll();
bool storageLoad();
void handleSetupButton(uint8_t buttonNum);
void setupFieldEnter();
void setupFieldDisplay();
void handleEncoderRotation(int delta);
void handleEncoderClick();
void handleEncoderLongPress();
void setupSave();
void setupDiscard();
void resolveLedConflicts(uint8_t savedIdx);
void normalizeLedAssignments();

// --- Motor activation helper: centralized motor start ---
void motorActivate(Turnout &t)
{
  t.motorActive = true;
  t.motorStartMs = millis();
}

// --- LCD helpers ---
const char *getTurnoutLabel(uint8_t index, char *buf, uint8_t bufSize)
{
  if (turnouts[index].name != nullptr && turnouts[index].name[0] != '\0')
    return turnouts[index].name;
  snprintf(buf, bufSize, "T%u", index);
  return buf;
}

void lcdShowIdle()
{
  uint8_t straight = 0;
  uint8_t turn = 0;
  for (uint8_t i = 0; i < NUM_BUTTONS; i++)
  {
    if (!turnouts[i].configured)
      continue;
    if (turnouts[i].state == STRAIGHT)
      straight++;
    else
      turn++;
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SwitchCtrl");
  lcd.setCursor(0, 1);
  lcd.print("S:");
  lcd.print(straight);
  lcd.print(" T:");
  lcd.print(turn);
  lcdShowingAction = false;
}

// --- Lookup: find configured turnout by button number ---
Turnout *findTurnoutByButton(uint8_t buttonNum)
{
  for (uint8_t i = 0; i < NUM_BUTTONS; i++)
  {
    if (turnouts[i].configured && turnouts[i].buttonIndex == buttonNum)
      return &turnouts[i];
  }
  return nullptr;
}

// --- Helper: configure a turnout with sensible defaults ---
void configureTurnout(uint8_t index, uint8_t buttonIndex, uint8_t in1, uint8_t in2,
                      uint8_t inLed, uint8_t straightLed, uint8_t turnLed, bool reversed, const char *name)
{
  turnouts[index] = {buttonIndex, in1, in2, STRAIGHT,
                     inLed, straightLed, turnLed,
                     true, reversed, false, 0, 0, false, name};
}

// --- LED rendering: set all LEDs based on current turnout state ---
void renderAllTurnoutLeds()
{
  // Clear entire LED buffer to avoid stale LEDs after index remapping
  fill_solid(leds, NUM_LEDS, CRGB::Black);

  for (uint8_t i = 0; i < NUM_BUTTONS; i++)
  {
    if (!turnouts[i].configured)
      continue;

    if (turnouts[i].inLedIdx != LED_UNASSIGNED && turnouts[i].inLedIdx < NUM_LEDS)
      leds[turnouts[i].inLedIdx] = CRGB::Green;

    if (turnouts[i].state == STRAIGHT)
    {
      if (turnouts[i].straightLedIdx != LED_UNASSIGNED && turnouts[i].straightLedIdx < NUM_LEDS) leds[turnouts[i].straightLedIdx] = CRGB::Green;
      if (turnouts[i].turnLedIdx != LED_UNASSIGNED && turnouts[i].turnLedIdx < NUM_LEDS) leds[turnouts[i].turnLedIdx] = CRGB::Red;
    }
    else
    {
      if (turnouts[i].straightLedIdx != LED_UNASSIGNED && turnouts[i].straightLedIdx < NUM_LEDS) leds[turnouts[i].straightLedIdx] = CRGB::Red;
      if (turnouts[i].turnLedIdx != LED_UNASSIGNED && turnouts[i].turnLedIdx < NUM_LEDS) leds[turnouts[i].turnLedIdx] = CRGB::Green;
    }
  }
  FastLED.setBrightness(brightness);
  FastLED.show();
}

// --- FRAM: low-level byte access over SoftwareWire ---
// MB85RC256V uses a 16-bit memory address word (MSB first).
uint8_t framRead8(uint16_t addr)
{
  fram.beginTransmission(FRAM_I2C_ADDR);
  fram.write((uint8_t)(addr >> 8));
  fram.write((uint8_t)(addr & 0xFF));
  fram.endTransmission(false); // RESTART, keep bus
  fram.requestFrom((uint8_t)FRAM_I2C_ADDR, (uint8_t)1);
  if (fram.available())
    return (uint8_t)fram.read();
  return 0xFF;
}

// Returns the SoftwareWire endTransmission() status: 0 = success, non-zero
// indicates an I2C bus error (NACK on address, NACK on data, timeout, etc.).
// Callers can OR multiple write statuses together to detect any failure
// across a multi-byte save.
uint8_t framWrite8(uint16_t addr, uint8_t val)
{
  fram.beginTransmission(FRAM_I2C_ADDR);
  fram.write((uint8_t)(addr >> 8));
  fram.write((uint8_t)(addr & 0xFF));
  fram.write(val);
  return fram.endTransmission();
}

// Write-then-read a scratch byte above our persistent region to verify the
// FRAM is wired up and responds correctly. Always restores the original
// byte before returning, including on failure paths.
bool framProbe()
{
  const uint16_t probeAddr = 0x7FFF; // last byte of 32KB
  uint8_t original = framRead8(probeAddr);
  const uint8_t pattern1 = 0xA5;
  const uint8_t pattern2 = 0x5A;
  bool ok = true;

  framWrite8(probeAddr, pattern1);
  if (framRead8(probeAddr) != pattern1)
  {
    ok = false;
  }
  else
  {
    framWrite8(probeAddr, pattern2);
    if (framRead8(probeAddr) != pattern2)
      ok = false;
  }

  framWrite8(probeAddr, original);
  return ok;
}

// --- FRAM: CRC8 over a range of storage bytes ---
uint8_t storageCRC8(uint8_t startAddr, uint8_t length)
{
  uint8_t crc = 0;
  for (uint8_t i = 0; i < length; i++)
  {
    uint8_t b = framRead8(startAddr + i);
    for (uint8_t bit = 0; bit < 8; bit++)
    {
      uint8_t mix = (crc ^ b) & 0x01;
      crc >>= 1;
      if (mix)
        crc ^= 0x8C;
      b >>= 1;
    }
  }
  return crc;
}

// --- FRAM: save a single turnout's config ---
void storageSaveTurnout(uint8_t index)
{
  if (!framAvailable) return;
  uint8_t addr = STORAGE_ADDR_DATA + (index * STORAGE_BYTES_PER_TURNOUT);
  // Encode state in bit 0, reversed in bit 1
  uint8_t stateByte = (uint8_t)turnouts[index].state | (turnouts[index].reversed ? 0x02 : 0x00);
  // Capture the first non-zero I2C status so the logged code is a real
  // endTransmission() result rather than a meaningless OR of several.
  uint8_t err = 0;
  uint8_t s;
  if (!err && (s = framWrite8(addr + 0, stateByte))) err = s;
  if (!err && (s = framWrite8(addr + 1, turnouts[index].inLedIdx))) err = s;
  if (!err && (s = framWrite8(addr + 2, turnouts[index].straightLedIdx))) err = s;
  if (!err && (s = framWrite8(addr + 3, turnouts[index].turnLedIdx))) err = s;
  // Recompute and save CRC over entire data block
  uint8_t crc = storageCRC8(STORAGE_ADDR_DATA, STORAGE_BYTES_PER_TURNOUT * NUM_BUTTONS);
  if (!err && (s = framWrite8(STORAGE_ADDR_CRC, crc))) err = s;
  if (err)
  {
    Serial.print("FRAM: save failed for turnout ");
    Serial.print(index);
    Serial.print(" (I2C status ");
    Serial.print(err);
    Serial.println(")");
  }
}

// --- FRAM: save all turnouts + header ---
void storageSaveAll()
{
  if (!framAvailable) return;
  uint8_t err = 0;
  uint8_t s;
  if (!err && (s = framWrite8(STORAGE_ADDR_MAGIC, STORAGE_MAGIC))) err = s;
  if (!err && (s = framWrite8(STORAGE_ADDR_VERSION, STORAGE_VERSION))) err = s;
  for (uint8_t i = 0; i < NUM_BUTTONS; i++)
  {
    uint8_t addr = STORAGE_ADDR_DATA + (i * STORAGE_BYTES_PER_TURNOUT);
    uint8_t stateByte = (uint8_t)turnouts[i].state | (turnouts[i].reversed ? 0x02 : 0x00);
    if (!err && (s = framWrite8(addr + 0, stateByte))) err = s;
    if (!err && (s = framWrite8(addr + 1, turnouts[i].inLedIdx))) err = s;
    if (!err && (s = framWrite8(addr + 2, turnouts[i].straightLedIdx))) err = s;
    if (!err && (s = framWrite8(addr + 3, turnouts[i].turnLedIdx))) err = s;
  }
  uint8_t crc = storageCRC8(STORAGE_ADDR_DATA, STORAGE_BYTES_PER_TURNOUT * NUM_BUTTONS);
  if (!err && (s = framWrite8(STORAGE_ADDR_CRC, crc))) err = s;
  if (err)
  {
    Serial.print("FRAM: saveAll failed (I2C status ");
    Serial.print(err);
    Serial.println(")");
  }
  else
  {
    Serial.println("FRAM: saved all");
  }
}

// --- FRAM: load turnout config; returns true if valid data was loaded ---
bool storageLoad()
{
  if (!framAvailable) return false;
  if (framRead8(STORAGE_ADDR_MAGIC) != STORAGE_MAGIC ||
      framRead8(STORAGE_ADDR_VERSION) != STORAGE_VERSION)
  {
    Serial.println("FRAM: no valid header");
    return false;
  }

  uint8_t storedCrc = framRead8(STORAGE_ADDR_CRC);
  uint8_t calcCrc = storageCRC8(STORAGE_ADDR_DATA, STORAGE_BYTES_PER_TURNOUT * NUM_BUTTONS);
  if (storedCrc != calcCrc)
  {
    Serial.println("FRAM: CRC mismatch");
    return false;
  }

  for (uint8_t i = 0; i < NUM_BUTTONS; i++)
  {
    if (!turnouts[i].configured)
      continue;
    uint8_t addr = STORAGE_ADDR_DATA + (i * STORAGE_BYTES_PER_TURNOUT);
    uint8_t stateByte = framRead8(addr + 0);
    uint8_t inLed = framRead8(addr + 1);
    uint8_t straightLed = framRead8(addr + 2);
    uint8_t turnLed = framRead8(addr + 3);

    // Decode state (bit 0) and reversed (bit 1)
    uint8_t state = stateByte & 0x01;
    bool reversed = (stateByte & 0x02) != 0;

    // Validate ranges — state byte must have no unexpected bits set
    // LED indices must be < NUM_LEDS or LED_UNASSIGNED (0xFF)
    if ((stateByte & 0xFC) != 0 ||
        (inLed >= NUM_LEDS && inLed != LED_UNASSIGNED) ||
        (straightLed >= NUM_LEDS && straightLed != LED_UNASSIGNED) ||
        (turnLed >= NUM_LEDS && turnLed != LED_UNASSIGNED))
    {
      Serial.print("FRAM: invalid data for turnout ");
      Serial.println(i);
      continue;
    }

    turnouts[i].state = (SwitchState)state;
    turnouts[i].reversed = reversed;
    turnouts[i].inLedIdx = inLed;
    turnouts[i].straightLedIdx = straightLed;
    turnouts[i].turnLedIdx = turnLed;
  }

  Serial.println("FRAM: loaded");
  return true;
}

// --- Setup mode: display current field on LCD ---
void setupFieldDisplay()
{
  if (setupTurnoutIdx < 0)
    return;

  char labelBuf[8];
  const char *label = getTurnoutLabel(setupTurnoutIdx, labelBuf, sizeof(labelBuf));
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(label);
  lcd.setCursor(0, 1);

  switch (setupField)
  {
  case SETUP_DIRECTION:
    lcd.print("Dir: ");
    lcd.print(setupDirection == STRAIGHT ? "STRAIGHT" : "TURN");
    break;
  case SETUP_IN_LED:
    lcd.print("InLED: ");
    if (setupInLed == LED_UNASSIGNED) lcd.print("--"); else lcd.print(setupInLed);
    break;
  case SETUP_STRAIGHT_LED:
    lcd.print("StrtLED: ");
    if (setupStraightLed == LED_UNASSIGNED) lcd.print("--"); else lcd.print(setupStraightLed);
    break;
  case SETUP_TURN_LED:
    lcd.print("TurnLED: ");
    if (setupTurnLed == LED_UNASSIGNED) lcd.print("--"); else lcd.print(setupTurnLed);
    break;
  default:
    break;
  }
}

// --- Setup mode: enter a field (called on transition) ---
void setupFieldEnter()
{
  // Reset blink state
  blinkLastToggleMs = millis();
  blinkOn = true;

  // Restore all LEDs to canonical state before entering new field
  renderAllTurnoutLeds();

  if (setupField == SETUP_DIRECTION && setupTurnoutIdx >= 0)
  {
    // Always reset to STRAIGHT — we fire driveStraight() so the user
    // can observe the position and relabel if needed
    setupDirection = STRAIGHT;

    // Fire motor in the "straight" direction so user can observe
    Turnout &t = turnouts[setupTurnoutIdx];
    driveStraight(t);
    motorActivate(t);
  }

  setupFieldDisplay();
}

// --- Setup mode: handle encoder rotation ---
void handleEncoderRotation(int delta)
{
  if (setupTurnoutIdx < 0 || setupField == SETUP_IDLE)
    return;

  // Ignore rotation while motor is active
  if (turnouts[setupTurnoutIdx].motorActive)
    return;

  switch (setupField)
  {
  case SETUP_DIRECTION:
    setupDirection = (setupDirection == STRAIGHT) ? TURN : STRAIGHT;
    break;
  case SETUP_IN_LED:
    if (setupInLed == LED_UNASSIGNED)
      setupInLed = (delta > 0) ? 0 : NUM_LEDS - 1;
    else if (setupInLed == 0 && delta < 0)
      setupInLed = LED_UNASSIGNED;
    else if (setupInLed == NUM_LEDS - 1 && delta > 0)
      setupInLed = LED_UNASSIGNED;
    else
      setupInLed = constrain((int)setupInLed + delta, 0, NUM_LEDS - 1);
    break;
  case SETUP_STRAIGHT_LED:
    if (setupStraightLed == LED_UNASSIGNED)
      setupStraightLed = (delta > 0) ? 0 : NUM_LEDS - 1;
    else if (setupStraightLed == 0 && delta < 0)
      setupStraightLed = LED_UNASSIGNED;
    else if (setupStraightLed == NUM_LEDS - 1 && delta > 0)
      setupStraightLed = LED_UNASSIGNED;
    else
      setupStraightLed = constrain((int)setupStraightLed + delta, 0, NUM_LEDS - 1);
    break;
  case SETUP_TURN_LED:
    if (setupTurnLed == LED_UNASSIGNED)
      setupTurnLed = (delta > 0) ? 0 : NUM_LEDS - 1;
    else if (setupTurnLed == 0 && delta < 0)
      setupTurnLed = LED_UNASSIGNED;
    else if (setupTurnLed == NUM_LEDS - 1 && delta > 0)
      setupTurnLed = LED_UNASSIGNED;
    else
      setupTurnLed = constrain((int)setupTurnLed + delta, 0, NUM_LEDS - 1);
    break;
  default:
    break;
  }

  // Restore LEDs and reset blink for LED fields
  renderAllTurnoutLeds();
  blinkLastToggleMs = millis();
  blinkOn = true;

  setupFieldDisplay();
}

// --- Setup mode: handle encoder short click (advance field) ---
void handleEncoderClick()
{
  if (appMode == MODE_NORMAL)
    return;

  if (setupField == SETUP_IDLE || setupTurnoutIdx < 0)
    return;

  // Advance to next field (cycle)
  switch (setupField)
  {
  case SETUP_DIRECTION:
    setupField = SETUP_IN_LED;
    break;
  case SETUP_IN_LED:
    setupField = SETUP_STRAIGHT_LED;
    break;
  case SETUP_STRAIGHT_LED:
    setupField = SETUP_TURN_LED;
    break;
  case SETUP_TURN_LED:
    setupField = SETUP_DIRECTION;
    break;
  default:
    break;
  }

  setupFieldEnter();
}

// --- Setup mode: save current config to turnout and FRAM ---
void setupSave()
{
  if (setupTurnoutIdx < 0)
    return;

  Turnout &t = turnouts[setupTurnoutIdx];

  // We showed the user the STRAIGHT position via driveStraight().
  // If they relabeled it as TURN, toggle the reversed flag so motor
  // commands align with the user's labeling.
  if (setupDirection == TURN)
    t.reversed = !t.reversed;

  t.state = setupDirection;
  t.inLedIdx = setupInLed;
  t.straightLedIdx = setupStraightLed;
  t.turnLedIdx = setupTurnLed;

  // Resolve LED conflicts: self-conflicts and cross-turnout duplicates
  resolveLedConflicts(setupTurnoutIdx);

  storageSaveTurnout(setupTurnoutIdx);

  Serial.print("Setup: saved turnout ");
  Serial.print(setupTurnoutIdx);
  Serial.print(t.reversed ? " (reversed)" : " (normal)");
  Serial.println();

  renderAllTurnoutLeds();
}

// --- Setup mode: discard unsaved changes and restore physical state ---
void setupDiscard()
{
  if (setupTurnoutIdx < 0)
    return;

  // If we physically moved the turnout on SETUP_DIRECTION entry,
  // drive it back to its saved state
  Turnout &t = turnouts[setupTurnoutIdx];
  if (t.state == TURN)
  {
    // We fired driveStraight on entry but the saved state is TURN —
    // restore the physical position
    driveTurn(t);
    motorActivate(t);
  }

  Serial.print("Setup: discarded turnout ");
  Serial.println(setupTurnoutIdx);

  renderAllTurnoutLeds();
}

// --- LED conflict resolution: clear any LED index from other turnouts ---
void resolveLedConflicts(uint8_t savedIdx)
{
  // Collect the saved turnout's 3 LED indices
  uint8_t savedLeds[3] = {
    turnouts[savedIdx].inLedIdx,
    turnouts[savedIdx].straightLedIdx,
    turnouts[savedIdx].turnLedIdx
  };

  // Self-conflict: later field wins (IN < STRAIGHT < TURN)
  // Check IN vs STRAIGHT
  if (savedLeds[0] != LED_UNASSIGNED && savedLeds[0] == savedLeds[1])
  {
    turnouts[savedIdx].inLedIdx = LED_UNASSIGNED;
    savedLeds[0] = LED_UNASSIGNED;
  }
  // Check IN vs TURN
  if (savedLeds[0] != LED_UNASSIGNED && savedLeds[0] == savedLeds[2])
  {
    turnouts[savedIdx].inLedIdx = LED_UNASSIGNED;
    savedLeds[0] = LED_UNASSIGNED;
  }
  // Check STRAIGHT vs TURN
  if (savedLeds[1] != LED_UNASSIGNED && savedLeds[1] == savedLeds[2])
  {
    turnouts[savedIdx].straightLedIdx = LED_UNASSIGNED;
    savedLeds[1] = LED_UNASSIGNED;
  }

  // Cross-turnout: clear matching fields on other turnouts
  for (uint8_t j = 0; j < NUM_BUTTONS; j++)
  {
    if (j == savedIdx || !turnouts[j].configured)
      continue;

    bool dirty = false;
    for (uint8_t k = 0; k < 3; k++)
    {
      if (savedLeds[k] == LED_UNASSIGNED)
        continue;

      if (turnouts[j].inLedIdx == savedLeds[k])
      {
        turnouts[j].inLedIdx = LED_UNASSIGNED;
        dirty = true;
      }
      if (turnouts[j].straightLedIdx == savedLeds[k])
      {
        turnouts[j].straightLedIdx = LED_UNASSIGNED;
        dirty = true;
      }
      if (turnouts[j].turnLedIdx == savedLeds[k])
      {
        turnouts[j].turnLedIdx = LED_UNASSIGNED;
        dirty = true;
      }
    }

    if (dirty)
      storageSaveTurnout(j);
  }
}

// --- Startup: resolve any pre-existing LED duplicates across all turnouts ---
void normalizeLedAssignments()
{
  // Track which LED index is claimed. Higher turnout index wins.
  // Value = turnout index that owns this LED, or 0xFF if unclaimed.
  uint8_t owner[NUM_LEDS];
  memset(owner, 0xFF, NUM_LEDS);

  for (uint8_t i = 0; i < NUM_BUTTONS; i++)
  {
    if (!turnouts[i].configured)
      continue;

    uint8_t *fields[3] = {
      &turnouts[i].inLedIdx,
      &turnouts[i].straightLedIdx,
      &turnouts[i].turnLedIdx
    };

    for (uint8_t f = 0; f < 3; f++)
    {
      uint8_t idx = *fields[f];
      if (idx == LED_UNASSIGNED || idx >= NUM_LEDS)
        continue;

      if (owner[idx] != 0xFF && owner[idx] != i)
      {
        // Conflict: clear this field from the previous owner
        Turnout &prev = turnouts[owner[idx]];
        if (prev.inLedIdx == idx) prev.inLedIdx = LED_UNASSIGNED;
        if (prev.straightLedIdx == idx) prev.straightLedIdx = LED_UNASSIGNED;
        if (prev.turnLedIdx == idx) prev.turnLedIdx = LED_UNASSIGNED;
        storageSaveTurnout(owner[idx]);
      }
      owner[idx] = i;
    }
  }
}

// --- Setup mode: handle encoder long-press (toggle setup mode) ---
void handleEncoderLongPress()
{
  if (appMode == MODE_NORMAL)
  {
    appMode = MODE_SETUP;
    setupField = SETUP_IDLE;
    setupTurnoutIdx = -1;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Setup Mode");
    lcd.setCursor(0, 1);
    lcd.print("Press button");
    lcdShowingAction = false;
    Serial.println("Entered setup mode");
  }
  else
  {
    // Discard any unsaved changes
    if (setupField != SETUP_IDLE)
      setupDiscard();

    appMode = MODE_NORMAL;
    setupField = SETUP_IDLE;
    setupTurnoutIdx = -1;
    renderAllTurnoutLeds();
    lcdShowIdle();
    Serial.println("Exited setup mode");
  }
}

// --- Setup mode: handle button matrix press ---
void handleSetupButton(uint8_t buttonNum)
{
  Turnout *t = findTurnoutByButton(buttonNum);
  if (t == nullptr)
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Btn ");
    lcd.print(buttonNum);
    lcd.setCursor(0, 1);
    lcd.print("Not configured");
    return;
  }

  uint8_t tIdx = t - turnouts;

  if (setupField != SETUP_IDLE && buttonNum == setupButtonNum)
  {
    // Same button pressed again — save and return to idle
    setupSave();
    setupField = SETUP_IDLE;
    setupTurnoutIdx = -1;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Saved!");
    lcd.setCursor(0, 1);
    lcd.print("Press button");
    return;
  }

  // Different button or first selection — discard previous and start new
  if (setupField != SETUP_IDLE)
    setupDiscard();

  setupTurnoutIdx = tIdx;
  setupButtonNum = buttonNum;

  // Load current values into setup temporaries
  // Direction always starts as STRAIGHT because we fire driveStraight() on entry
  // to show the user what the system thinks STRAIGHT is
  setupDirection = STRAIGHT;
  setupInLed = t->inLedIdx;
  setupStraightLed = t->straightLedIdx;
  setupTurnLed = t->turnLedIdx;

  setupField = SETUP_DIRECTION;
  setupFieldEnter();

  Serial.print("Setup: configuring turnout ");
  Serial.println(tIdx);
}

// --- Callback: fires during matrix.update() on button release (click) ---
void onButtonAction(Button &button)
{
  if (button.getLastAction() != BTN_ACTION_CLICK)
    return;

  uint8_t btnNum = button.getNumber();

  // Route to setup handler if in setup mode
  if (appMode == MODE_SETUP)
  {
    handleSetupButton(btnNum);
    return;
  }

  Serial.print("Button ");
  Serial.print(btnNum);

  Turnout *t = findTurnoutByButton(btnNum);
  if (t == nullptr) {
    Serial.println(" not configured");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Btn ");
    lcd.print(btnNum);
    lcd.setCursor(0, 1);
    lcd.print("Not configured");
    lcdMessageMs = millis();
    lcdShowingAction = true;
    return;
  }
  if (t->motorActive) {
    Serial.println(" motor active");
    return;
  }

  // Ignore rapid repeated clicks (cooldown), resetting the window each time
  unsigned long now = millis();
  if (t->lastClickMs > 0 &&
      (now - t->lastClickMs < CLICK_COOLDOWN_MS))
  {
    t->lastClickMs = now;
    Serial.println(" click ignored (cooldown)");
    return;
  }
  t->lastClickMs = now;

  Serial.println(" action");

  switch (t->state)
  {
  case STRAIGHT:
    driveTurn(*t);
    t->state = TURN;
    Serial.println("Switch set to TURN");
    break;
  case TURN:
    driveStraight(*t);
    t->state = STRAIGHT;
    Serial.println("Switch set to STRAIGHT");
    break;
  }

  renderAllTurnoutLeds();

  // Update LCD with action
  uint8_t tIdx = t - turnouts;
  char labelBuf[8];
  const char *label = getTurnoutLabel(tIdx, labelBuf, sizeof(labelBuf));
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(label);
  lcd.setCursor(0, 1);
  lcd.print("-> ");
  lcd.print(t->state == STRAIGHT ? "STRAIGHT" : "TURN");
  lcdMessageMs = millis();
  lcdShowingAction = true;

  // Start motor timer (non-blocking) — state saved to FRAM after motor completes in loop()
  t->pendingStorageSave = (appMode == MODE_NORMAL);
  motorActivate(*t);
}

void setup()
{
  Serial.begin(9600);

  // Initialize FastLED
  FastLED.addLeds<WS2812B, LED_PIN, RGB>(leds, NUM_LEDS);
  FastLED.setBrightness(brightness);

  // Define turnouts with hardcoded defaults
  // (index, buttonIndex, in1, in2, inLed, straightLed, turnLed, reversed, name)
  // Turnout 0 state=STRAIGHT btn=5 LEDs(in=15 st=17 tn=16) reversed=Y
  // Turnout 1 state=STRAIGHT btn=0 LEDs(in=0 st=2 tn=1) reversed=N
  // Turnout 2 state=STRAIGHT btn=4 LEDs(in=12 st=14 tn=13) reversed=Y
  // Turnout 3 state=STRAIGHT btn=1 LEDs(in=3 st=5 tn=4) reversed=N
  // Turnout 4 state=STRAIGHT btn=6 LEDs(in=18 st=20 tn=19) reversed=Y
  // Turnout 5 state=STRAIGHT btn=3 LEDs(in=9 st=11 tn=10) reversed=Y
  // Turnout 6 state=TURN btn=2 LEDs(in=8 st=6 tn=7) reversed=N
  // Turnout 7 state=STRAIGHT btn=9 LEDs(in=23 st=21 tn=22) reversed=N
  // Turnout 8 state=TURN btn=10 LEDs(in=24 st=26 tn=25) reversed=Y
  // Turnout 9 state=TURN btn=8 LEDs(in=30 st=32 tn=31) reversed=N
  // Turnout 10 state=TURN btn=11 LEDs(in=35 st=33 tn=34) reversed=Y
  // Turnout 11 state=TURN btn=7 LEDs(in=27 st=29 tn=28) reversed=N

  configureTurnout(0,  5,  T0_IN1,  T0_IN2,  15, 17, 16, true,  "Industry Entry");
  configureTurnout(1,  0,  T1_IN1,  T1_IN2,   0,  2,  1, false, "Industry Exit");
  configureTurnout(2,  4,  T2_IN1,  T2_IN2,  12, 14, 13, true,  "Siding Entry");
  configureTurnout(3,  1,  T3_IN1,  T3_IN2,   3,  5,  4, false, "Siding Exit");
  configureTurnout(4,  6,  T4_IN1,  T4_IN2,  18, 20, 19, true,  "Industry Warehouse");
  configureTurnout(5,  3,  T5_IN1,  T5_IN2,   9, 11, 10, true,  "Runaround Entry");
  configureTurnout(6,  2,  T6_IN1,  T6_IN2,   8,  6,  7, false, "Runaround Exit");
  configureTurnout(7,  9,  T7_IN1,  T7_IN2,  23, 21, 22, false, "Staging 2 & 3");
  configureTurnout(8, 10,  T8_IN1,  T8_IN2,  24, 26, 25, true,  "Staging 2/3 Entry");
  configureTurnout(9,  8,  T9_IN1,  T9_IN2,  30, 32, 31, false, "Yard Entry");
  configureTurnout(10, 11, T10_IN1, T10_IN2,  35, 33, 34, true,  "Yard Exit");
  configureTurnout(11,  7, T11_IN1, T11_IN2,  27, 29, 28, false, "Staging 1");

  // Initialize FRAM on the private software I2C bus (pins 14/15).
  // Hardware Wire is reserved for the DCC-EX slave role and cannot be shared.
  fram.begin();
  framAvailable = framProbe();
  if (framAvailable)
  {
    Serial.println("FRAM: ready");
  }
  else
  {
    Serial.println("FRAM: probe failed - persistence disabled");
  }

  // Load FRAM config BEFORE motor initialization
  // This overrides hardcoded state and LED indices with saved values
  if (!framAvailable)
  {
    Serial.println("FRAM: unavailable, using hardcoded defaults");
  }
  else if (!storageLoad())
  {
    Serial.println("FRAM: first boot, saving defaults");
    storageSaveAll();
  }

  // Resolve any pre-existing LED duplicates from older firmware or manual edits
  normalizeLedAssignments();

  // Initialize motor pins and drive turnouts to their saved state
  for (int i = 0; i < NUM_BUTTONS; i++)
  {
    if (!turnouts[i].configured)
      continue;

    pinMode(turnouts[i].in1, OUTPUT);
    pinMode(turnouts[i].in2, OUTPUT);

    Serial.print("Turnout ");
    Serial.print(i);
    Serial.print(" state=");
    Serial.print(turnouts[i].state == STRAIGHT ? "STRAIGHT" : "TURN");
    Serial.print(" btn=");
    Serial.print(turnouts[i].buttonIndex);
    Serial.print(" LEDs(in=");
    Serial.print(turnouts[i].inLedIdx);
    Serial.print(" st=");
    Serial.print(turnouts[i].straightLedIdx);
    Serial.print(" tn=");
    Serial.print(turnouts[i].turnLedIdx);
    Serial.print(") reversed=");
    Serial.println(turnouts[i].reversed ? "Y" : "N");

    // Drive motor to saved state (with safety timer backstop)
    if (turnouts[i].state == STRAIGHT)
      driveStraight(turnouts[i]);
    else
      driveTurn(turnouts[i]);
    motorActivate(turnouts[i]);

    delay(MOTOR_DURATION_MS);
    switchOff(turnouts[i].in1, turnouts[i].in2);
    turnouts[i].motorActive = false;
  }

  // Set LEDs to match loaded state
  renderAllTurnoutLeds();

  // Initialize I2C slave AFTER all motors are positioned
  i2cSlaveSetup();

  // Initialize LCD
  lcd.begin(16, 2);
  uint8_t configuredCount = 0;
  for (uint8_t i = 0; i < NUM_BUTTONS; i++)
  {
    if (turnouts[i].configured)
      configuredCount++;
  }
  lcd.setCursor(0, 0);
  lcd.print("SwitchCtrl Ready");
  lcd.setCursor(0, 1);
  lcd.print(configuredCount);
  lcd.print(" turnouts");

  // Initialize button matrix
  matrix.init();
  matrix.setScanInterval(20);
  matrix.setMinLongPressDuration(0xFFFF);
  matrix.registerButtonActionCallback(onButtonAction);

  // Initialize encoder switch pin
  pinMode(ENC_SW, INPUT_PULLUP);

  Serial.println("Setup complete");
}

void loop()
{
  // --- Scan buttons ---
  matrix.update();

  // Drain stale m_stateChanged flags (library bug workaround)
  for (uint8_t i = 0; i < NUM_BUTTONS; i++)
  {
    buttons[i].hasStateChanged();
  }

  // --- Rotary encoder rotation ---
  long encPos = encoder.read();
  if (encPos != encLastPos)
  {
    // Encoder library counts 4 steps per detent — process all whole detents
    long diff = encPos - encLastPos;
    long detents = diff / 4;
    if (detents != 0)
    {
      encLastPos += detents * 4; // Preserve sub-detent remainder
      if (appMode == MODE_SETUP)
        handleEncoderRotation(-(int)detents);
    }
  }

  // --- Rotary encoder switch (polled with debounce) ---
  bool swReading = digitalRead(ENC_SW);
  if (swReading != encSwLastState)
  {
    encSwDebounceMs = millis();
  }
  encSwLastState = swReading;

  if ((millis() - encSwDebounceMs) >= ENC_DEBOUNCE_MS)
  {
    if (swReading == LOW && !encSwPressed)
    {
      // Button just pressed down
      encSwPressed = true;
      encSwPressStartMs = millis();
      encLongPressHandled = false;
    }
    else if (swReading == HIGH && encSwPressed)
    {
      // Button just released
      encSwPressed = false;
      if (!encLongPressHandled)
      {
        // Short click
        handleEncoderClick();
      }
    }
  }

  // Long-press detection while held
  if (encSwPressed && !encLongPressHandled &&
      (millis() - encSwPressStartMs >= ENC_LONG_PRESS_MS))
  {
    encLongPressHandled = true;
    handleEncoderLongPress();
  }

  // --- Process one I2C command from DCC-EX per loop iteration ---
  processI2CCommands();
  updateI2CStateSnapshot();

  // --- Non-blocking motor shutoff + FRAM state save ---
  for (uint8_t i = 0; i < NUM_BUTTONS; i++)
  {
    // Normal shutoff path
    if (turnouts[i].motorActive &&
        (millis() - turnouts[i].motorStartMs >= MOTOR_DURATION_MS))
    {
      switchOff(turnouts[i].in1, turnouts[i].in2);
      turnouts[i].motorActive = false;
    }

    // Save to FRAM once motor is no longer active
    if (turnouts[i].pendingStorageSave && !turnouts[i].motorActive)
    {
      storageSaveTurnout(i);
      turnouts[i].pendingStorageSave = false;
    }
  }

  // --- LED blink during setup LED fields ---
  if (appMode == MODE_SETUP && setupTurnoutIdx >= 0 &&
      (setupField == SETUP_IN_LED || setupField == SETUP_STRAIGHT_LED || setupField == SETUP_TURN_LED))
  {
    if ((millis() - blinkLastToggleMs) >= BLINK_INTERVAL_MS)
    {
      blinkLastToggleMs = millis();
      blinkOn = !blinkOn;

      uint8_t blinkIdx = 0;
      switch (setupField)
      {
      case SETUP_IN_LED:
        blinkIdx = setupInLed;
        break;
      case SETUP_STRAIGHT_LED:
        blinkIdx = setupStraightLed;
        break;
      case SETUP_TURN_LED:
        blinkIdx = setupTurnLed;
        break;
      default:
        break;
      }

      if (blinkIdx != LED_UNASSIGNED && blinkIdx < NUM_LEDS)
      {
        leds[blinkIdx] = blinkOn ? CRGB::White : CRGB::Black;
        FastLED.setBrightness(brightness);
        FastLED.show();
      }
    }
  }

  // --- Revert LCD to idle summary after timeout (normal mode only) ---
  if (appMode == MODE_NORMAL && lcdShowingAction &&
      (millis() - lcdMessageMs >= LCD_MESSAGE_MS))
  {
    lcdShowIdle();
  }
}

void switchOff(int in1, int in2)
{
  digitalWrite(in1, LOW);
  digitalWrite(in2, LOW);
}

void switchStraight(int in1, int in2)
{
  digitalWrite(in1, HIGH);
  digitalWrite(in2, LOW);
}

void switchTurn(int in1, int in2)
{
  digitalWrite(in1, LOW);
  digitalWrite(in2, HIGH);
}

// Drive turnout to STRAIGHT, accounting for reversed polarity
void driveStraight(Turnout &t)
{
  if (t.reversed)
    switchTurn(t.in1, t.in2);
  else
    switchStraight(t.in1, t.in2);
}

// Drive turnout to TURN, accounting for reversed polarity
void driveTurn(Turnout &t)
{
  if (t.reversed)
    switchStraight(t.in1, t.in2);
  else
    switchTurn(t.in1, t.in2);
}