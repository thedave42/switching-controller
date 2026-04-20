#ifndef TURNOUT_TYPES_H
#define TURNOUT_TYPES_H

#include <Arduino.h>

// --- Enums ---
enum SwitchState
{
  STRAIGHT,
  TURN
};

enum AppMode
{
  MODE_NORMAL,
  MODE_SETUP
};

enum SetupField
{
  SETUP_IDLE,
  SETUP_DIRECTION,
  SETUP_IN_LED,
  SETUP_STRAIGHT_LED,
  SETUP_TURN_LED
};

// --- Turnout configuration ---
struct Turnout
{
  int buttonIndex;
  int in1;
  int in2;
  SwitchState state;
  uint8_t inLedIdx;
  uint8_t straightLedIdx;
  uint8_t turnLedIdx;
  bool configured;
  bool reversed;
  bool motorActive;
  unsigned long motorStartMs;
  unsigned long lastClickMs;
  bool pendingEepromSave;
  const char *name;
};

#endif // TURNOUT_TYPES_H
