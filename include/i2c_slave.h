#ifndef I2C_SLAVE_H
#define I2C_SLAVE_H

#include <Arduino.h>
#include "i2c_protocol.h"
#include "turnout_types.h"

// Initialize I2C slave on the configured address.
// Must be called AFTER motor initialization completes in setup().
void i2cSlaveSetup();

// Process queued I2C write commands in the main loop context.
// Dequeues commands from the ring buffer and drives turnout motors.
void processI2CCommands();

// Update the cached state/busy bitmasks that are sent in response to SC_READALL.
// Should be called every loop() iteration.
void updateI2CStateSnapshot();

#endif // I2C_SLAVE_H
