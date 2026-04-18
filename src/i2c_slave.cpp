#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal.h>
#include "i2c_protocol.h"
#include "i2c_slave.h"
#include "pinLayout.h"
#include "turnout_types.h"

// --- Externs from main.cpp ---
extern Turnout turnouts[];
extern AppMode appMode;
extern void driveStraight(Turnout &t);
extern void driveTurn(Turnout &t);
extern void motorActivate(Turnout &t);
extern void renderAllTurnoutLeds();
extern void lcdShowIdle();
extern LiquidCrystal lcd;
extern unsigned long lcdMessageMs;
extern bool lcdShowingAction;
extern const char *getTurnoutLabel(uint8_t index, char *buf, uint8_t bufSize);

// --- I2C state ---
static volatile I2CWriteCmd i2cCmdQueue[I2C_CMD_QUEUE_SIZE];
static volatile uint8_t i2cCmdHead = 0;
static volatile uint8_t i2cCmdTail = 0;

static volatile uint8_t outboundFlag = 0;
static volatile uint8_t i2cWriteResponseByte = SC_ERR;

// Pre-built SC_READALL response buffer (updated atomically in main loop)
static volatile uint8_t readallResponse[4] = {0, 0, 0, 0};

// Ready flag — set after startup motor init completes
static volatile bool i2cReady = false;

// Count of configured turnouts (cached at setup time)
static uint8_t configuredCount = 0;

// --- ISR: called when master sends data to this slave ---
static void receiveEvent(int numBytes)
{
  if (numBytes == 0)
    return;

  uint8_t buffer[4];
  uint8_t len = (numBytes < (int)sizeof(buffer)) ? (uint8_t)numBytes : (uint8_t)sizeof(buffer);
  for (uint8_t i = 0; i < len; i++)
    buffer[i] = Wire.read();
  // Drain any excess bytes
  while (Wire.available())
    Wire.read();

  switch (buffer[0])
  {
  case SC_GETINFO:
    outboundFlag = SC_GETINFO;
    break;

  case SC_GETVER:
    outboundFlag = SC_GETVER;
    break;

  case SC_WRITE:
    if (len >= 3)
    {
      uint8_t pin = buffer[1];
      uint8_t state = buffer[2];

      // Validate pin and state in ISR for immediate response
      if (pin < NUM_BUTTONS && turnouts[pin].configured &&
          state <= SC_STATE_TURN && !turnouts[pin].motorActive)
      {
        // Push to ring buffer
        uint8_t nextHead = (i2cCmdHead + 1) % I2C_CMD_QUEUE_SIZE;
        if (nextHead != i2cCmdTail)
        {
          i2cCmdQueue[i2cCmdHead].pin = pin;
          i2cCmdQueue[i2cCmdHead].state = state;
          i2cCmdHead = nextHead;
          i2cWriteResponseByte = SC_RDY;
        }
        else
        {
          i2cWriteResponseByte = SC_ERR; // Queue full
        }
      }
      else
      {
        i2cWriteResponseByte = SC_ERR;
      }
    }
    else
    {
      i2cWriteResponseByte = SC_ERR;
    }
    outboundFlag = SC_WRITE;
    break;

  case SC_READALL:
    outboundFlag = SC_READALL;
    break;

  default:
    outboundFlag = 0;
    break;
  }
}

// --- ISR: called when master requests data from this slave ---
static void requestEvent()
{
  switch (outboundFlag)
  {
  case SC_GETINFO:
  {
    uint8_t buf[4];
    buf[0] = i2cReady ? configuredCount : 0;
    buf[1] = SC_PROTOCOL_VER;
    buf[2] = 0x00; // featureFlags reserved
    buf[3] = 0x00; // reserved
    Wire.write(buf, 4);
    break;
  }

  case SC_GETVER:
  {
    uint8_t buf[3];
    buf[0] = SC_VERSION_MAJOR;
    buf[1] = SC_VERSION_MINOR;
    buf[2] = SC_VERSION_PATCH;
    Wire.write(buf, 3);
    break;
  }

  case SC_WRITE:
    Wire.write(i2cWriteResponseByte);
    break;

  case SC_READALL:
    Wire.write((const uint8_t *)readallResponse, 4);
    break;

  default:
    Wire.write(SC_ERR);
    break;
  }
}

// --- Public API ---

void i2cSlaveSetup()
{
  // Count configured turnouts
  configuredCount = 0;
  for (uint8_t i = 0; i < NUM_BUTTONS; i++)
  {
    if (turnouts[i].configured)
      configuredCount++;
  }

  Wire.begin(SC_I2C_ADDRESS);
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);

  i2cReady = true;

  Serial.print(F("I2C slave started at 0x"));
  Serial.println(SC_I2C_ADDRESS, HEX);
}

void processI2CCommands()
{
  // Process at most one command per loop iteration to avoid stalling
  // the main loop with multiple FastLED.show() + LCD writes
  if (i2cCmdHead == i2cCmdTail)
    return;

  I2CWriteCmd cmd;
  noInterrupts();
  cmd.pin = i2cCmdQueue[i2cCmdTail].pin;
  cmd.state = i2cCmdQueue[i2cCmdTail].state;
  i2cCmdTail = (i2cCmdTail + 1) % I2C_CMD_QUEUE_SIZE;
  interrupts();

  if (cmd.pin >= NUM_BUTTONS || !turnouts[cmd.pin].configured)
    return;

  Turnout &t = turnouts[cmd.pin];

  // Idempotent: skip if already in requested state
  if ((cmd.state == SC_STATE_STRAIGHT && t.state == STRAIGHT) ||
      (cmd.state == SC_STATE_TURN && t.state == TURN))
    return;

  // Skip if motor is busy
  if (t.motorActive)
    return;

  // Skip if in setup mode
  if (appMode != MODE_NORMAL)
    return;

  // Drive motor to requested state
  if (cmd.state == SC_STATE_TURN)
  {
    driveTurn(t);
    t.state = TURN;
  }
  else
  {
    driveStraight(t);
    t.state = STRAIGHT;
  }

  renderAllTurnoutLeds();
  t.pendingEepromSave = true;
  motorActivate(t);

  // LCD feedback
  uint8_t tIdx = cmd.pin;
  char labelBuf[8];
  const char *label = getTurnoutLabel(tIdx, labelBuf, sizeof(labelBuf));
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("DCC:"));
  lcd.print(label);
  lcd.setCursor(0, 1);
  lcd.print(F("-> "));
  lcd.print(t.state == STRAIGHT ? F("STRAIGHT") : F("TURN"));
  lcdMessageMs = millis();
  lcdShowingAction = true;

  Serial.print(F("I2C: turnout "));
  Serial.print(tIdx);
  Serial.print(F(" -> "));
  Serial.println(t.state == STRAIGHT ? F("STRAIGHT") : F("TURN"));
}

void updateI2CStateSnapshot()
{
  uint16_t states = 0;
  uint16_t busy = 0;

  for (uint8_t i = 0; i < NUM_BUTTONS; i++)
  {
    if (!turnouts[i].configured)
      continue;
    if (turnouts[i].state == TURN)
      states |= (1 << i);
    if (turnouts[i].motorActive)
      busy |= (1 << i);
  }

  noInterrupts();
  readallResponse[0] = states & 0xFF;
  readallResponse[1] = (states >> 8) & 0xFF;
  readallResponse[2] = busy & 0xFF;
  readallResponse[3] = (busy >> 8) & 0xFF;
  interrupts();
}
