#ifndef I2C_PROTOCOL_H
#define I2C_PROTOCOL_H

// I2C Protocol for Switching Controller <-> DCC-EX EX-CSB1 communication
//
// Master: EX-CSB1 (DCC-EX CommandStation) via I2CManager
// Slave:  Arduino Mega 2560 via Wire library
//
// Bus speed: 100kHz (standard mode)
// The "pin" byte in all commands refers to the turnout array index (0-11),
// NOT the button matrix number.

// --- Default I2C address ---
#define SC_I2C_ADDRESS  0x65

// --- Protocol version ---
#define SC_PROTOCOL_VER 1

// --- Commands (Master -> Slave) ---
#define SC_GETINFO  0xA0  // Get device info: responds with [numConfigured, protocolVer, featureFlags, reserved]
#define SC_GETVER   0xA1  // Get firmware version: responds with [major, minor, patch]
#define SC_WRITE    0xA2  // Set turnout state: [0xA2, pin, state] -> [SC_RDY] or [SC_ERR]
#define SC_READALL  0xA3  // Read all states: responds with [stateLo, stateHi, busyLo, busyHi]

// --- Responses (Slave -> Master) ---
#define SC_RDY      0xA9  // Acknowledge / ready
#define SC_ERR      0xAF  // Error / rejected

// --- Firmware version (Switching Controller) ---
#define SC_VERSION_MAJOR 1
#define SC_VERSION_MINOR 0
#define SC_VERSION_PATCH 0

// --- Write command states ---
#define SC_STATE_STRAIGHT 0
#define SC_STATE_TURN     1

// --- Ring buffer for I2C write commands (processed in main loop) ---
struct I2CWriteCmd {
  uint8_t pin;    // Turnout array index (0-11)
  uint8_t state;  // SC_STATE_STRAIGHT or SC_STATE_TURN
};

#define I2C_CMD_QUEUE_SIZE 8

#endif // I2C_PROTOCOL_H
