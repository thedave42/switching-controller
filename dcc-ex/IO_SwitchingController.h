/*
 *  IO_SwitchingController.h — DCC-EX HAL driver for the Switching Controller
 *
 *  This driver communicates over I2C with an Arduino Mega 2560 running the
 *  Switching Controller firmware, allowing DCC-EX to throw/close up to 12
 *  model railroad turnouts controlled by L298N H-bridge motor drivers.
 *
 *  The Switching Controller also supports local button control and LED feedback.
 *  State changes made locally are reflected in the next poll cycle.
 *
 *  Installation:
 *    1. Copy this file into the EX-CommandStation source folder
 *    2. Add to myHal.cpp:
 *         #include "IO_SwitchingController.h"
 *         void halSetup() {
 *           SwitchingController::create(800, 12, 0x65);
 *         }
 *    3. Define turnouts in mySetup.h or via serial commands:
 *         SETUP("<T 1 VPIN 800>");   // Turnout 1 on VPIN 800
 *         SETUP("<T 2 VPIN 801>");   // Turnout 2 on VPIN 801
 *         ...
 *    4. Control from any throttle:
 *         <T 1 T>   // Throw turnout 1
 *         <T 1 C>   // Close turnout 1
 *
 *  I2C Protocol (custom, modeled after EX-IOExpander):
 *    SC_GETINFO (0xA0) -> [numConfigured, protocolVer, featureFlags, reserved]
 *    SC_GETVER  (0xA1) -> [major, minor, patch]
 *    SC_WRITE   (0xA2) [pin, state] -> [SC_RDY] or [SC_ERR]
 *    SC_READALL (0xA3) -> [stateLo, stateHi, busyLo, busyHi]
 *
 *  Hardware requirements:
 *    - Bidirectional I2C level shifter (EX-CSB1 is 3.3V, Mega is 5V)
 *    - Common ground between both boards
 *    - Bus speed: 100kHz recommended
 *
 *  License: GPLv3 (same as DCC-EX)
 */

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
  // Protocol constants (duplicated from i2c_protocol.h — these two must stay in sync)
  enum {
    SC_GETINFO  = 0xA0,
    SC_GETVER   = 0xA1,
    SC_WRITE    = 0xA2,
    SC_READALL  = 0xA3,
    SC_RDY      = 0xA9,
    SC_ERR      = 0xAF,
  };

  SwitchingController(VPIN firstVpin, int nPins, I2CAddress i2cAddress) {
    _firstVpin = firstVpin;
    _nPins = min(nPins, 12);
    _I2CAddress = i2cAddress;
    addDevice(this);
  }

  void _begin() override {
    I2CManager.begin();
    if (!I2CManager.exists(_I2CAddress)) {
      DIAG(F("SwitchCtrl I2C:%s device not found"), _I2CAddress.toString());
      _deviceState = DEVSTATE_FAILED;
      return;
    }

    // Retry SC_GETINFO until the Mega reports ready (numConfigured > 0)
    uint8_t commandBuffer[1] = {SC_GETINFO};
    uint8_t receiveBuffer[4];
    bool ready = false;

    for (uint8_t attempt = 0; attempt < 10; attempt++) {
      uint8_t status = I2CManager.read(_I2CAddress, receiveBuffer, 4, commandBuffer, 1);
      if (status == I2C_STATUS_OK && receiveBuffer[0] > 0) {
        _numConfigured = receiveBuffer[0];
        ready = true;
        break;
      }
      delay(500);
    }

    if (!ready) {
      DIAG(F("SwitchCtrl I2C:%s not ready after retries"), _I2CAddress.toString());
      _deviceState = DEVSTATE_FAILED;
      return;
    }

    // Get firmware version
    commandBuffer[0] = SC_GETVER;
    uint8_t verBuffer[3];
    if (I2CManager.read(_I2CAddress, verBuffer, 3, commandBuffer, 1) == I2C_STATUS_OK) {
      _majorVer = verBuffer[0];
      _minorVer = verBuffer[1];
      _patchVer = verBuffer[2];
    }

    DIAG(F("SwitchCtrl I2C:%s v%d.%d.%d %d turnouts, Vpins %u-%u"),
         _I2CAddress.toString(), _majorVer, _minorVer, _patchVer,
         _numConfigured, (int)_firstVpin, (int)_firstVpin + _nPins - 1);
  }

  void _loop(unsigned long currentMicros) override {
    // If device is offline, retry every 5 seconds
    if (_deviceState == DEVSTATE_FAILED) {
      if (currentMicros - _lastRetryMicros < _retryIntervalUs)
        return;
      _lastRetryMicros = currentMicros;

      if (!I2CManager.exists(_I2CAddress))
        return;

      uint8_t commandBuffer[1] = {SC_GETINFO};
      uint8_t receiveBuffer[4];
      uint8_t status = I2CManager.read(_I2CAddress, receiveBuffer, 4, commandBuffer, 1);
      if (status == I2C_STATUS_OK && receiveBuffer[0] > 0) {
        _numConfigured = receiveBuffer[0];
        _deviceState = DEVSTATE_NORMAL;

        // Re-fetch version
        commandBuffer[0] = SC_GETVER;
        uint8_t verBuffer[3];
        if (I2CManager.read(_I2CAddress, verBuffer, 3, commandBuffer, 1) == I2C_STATUS_OK) {
          _majorVer = verBuffer[0];
          _minorVer = verBuffer[1];
          _patchVer = verBuffer[2];
        }

        DIAG(F("SwitchCtrl I2C:%s ONLINE v%d.%d.%d %d turnouts"),
             _I2CAddress.toString(), _majorVer, _minorVer, _patchVer, _numConfigured);
      }
      return;
    }

    // Normal operation: non-blocking poll for state changes
    if (_pollPending) {
      if (_i2crb.isBusy())
        return;

      if (_i2crb.status == I2C_STATUS_OK) {
        _statesMask = _pollResponseBuffer[0] | ((uint16_t)_pollResponseBuffer[1] << 8);
        _busyMask = _pollResponseBuffer[2] | ((uint16_t)_pollResponseBuffer[3] << 8);
      } else {
        _errorCount++;
        if (_errorCount > 10) {
          DIAG(F("SwitchCtrl I2C:%s too many errors, going OFFLINE"), _I2CAddress.toString());
          _deviceState = DEVSTATE_FAILED;
          _errorCount = 0;
        }
      }
      _pollPending = false;
    }

    if (currentMicros - _lastPollMicros >= _pollIntervalUs) {
      _pollCommandBuffer[0] = SC_READALL;
      I2CManager.read(_I2CAddress, _pollResponseBuffer, 4, _pollCommandBuffer, 1, &_i2crb);
      _lastPollMicros = currentMicros;
      _pollPending = true;
      _errorCount = 0;
    }
  }

  void _write(VPIN vpin, int value) override {
    if (_deviceState == DEVSTATE_FAILED) return;
    int pin = vpin - _firstVpin;
    if (pin < 0 || pin >= _nPins) return;

    uint8_t commandBuffer[3] = {SC_WRITE, (uint8_t)pin, (uint8_t)(value ? 1 : 0)};
    uint8_t responseBuffer[1];

    // Retry up to 3 times with 10ms backoff
    for (uint8_t attempt = 0; attempt < 3; attempt++) {
      uint8_t status = I2CManager.read(_I2CAddress, responseBuffer, 1, commandBuffer, 3);
      if (status == I2C_STATUS_OK) {
        if (responseBuffer[0] == SC_RDY) {
          // Update cached state
          if (value)
            _statesMask |= (1 << pin);
          else
            _statesMask &= ~(1 << pin);
          return;
        } else {
          return; // Device rejected the command (busy, invalid pin)
        }
      }
      delayMicroseconds(10000); // 10ms backoff
    }

    DIAG(F("SwitchCtrl I2C:%s write failed pin %d"), _I2CAddress.toString(), pin);
  }

  int _read(VPIN vpin) override {
    if (_deviceState == DEVSTATE_FAILED) return 0;
    int pin = vpin - _firstVpin;
    if (pin < 0 || pin >= _nPins) return 0;
    return (_statesMask >> pin) & 1;
  }

  void _display() override {
    DIAG(F("SwitchCtrl I2C:%s v%d.%d.%d Vpins:%u-%u %S"),
         _I2CAddress.toString(), _majorVer, _minorVer, _patchVer,
         (int)_firstVpin, (int)_firstVpin + _nPins - 1,
         _deviceState == DEVSTATE_FAILED ? F("OFFLINE") : F(""));
  }

  // Cached state from polling
  uint16_t _statesMask = 0;
  uint16_t _busyMask = 0;
  uint8_t _numConfigured = 0;
  uint8_t _majorVer = 0, _minorVer = 0, _patchVer = 0;
  uint8_t _errorCount = 0;

  // Non-blocking I2C for polling
  I2CRB _i2crb;
  uint8_t _pollCommandBuffer[1];
  uint8_t _pollResponseBuffer[4];
  unsigned long _lastPollMicros = 0;
  unsigned long _lastRetryMicros = 0;
  bool _pollPending = false;

  static const unsigned long _pollIntervalUs = 100000UL;    // 100ms
  static const unsigned long _retryIntervalUs = 5000000UL;  // 5s reconnect retry
};

#endif // IO_SWITCHINGCONTROLLER_H
