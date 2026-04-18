// myHal.cpp — DCC-EX HAL configuration for Switching Controller
//
// Copy this file and IO_SwitchingController.h to your EX-CommandStation
// source folder. See the Arduino IDE setup instructions at:
// https://dcc-ex.com/reference/developers/hal-config.html#adding-a-new-device

#include "IODevice.h"
#include "IO_SwitchingController.h"

void halSetup() {
  // Switching Controller: 12 turnout VPINs starting at 800, I2C address 0x65
  SwitchingController::create(800, 12, 0x65);
}
