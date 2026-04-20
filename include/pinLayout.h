#ifndef PINLAYOUT_H
#define PINLAYOUT_H

/*
ButtonMatrix Definitions
    Rows (INPUT_PULLUP):
        0: 12
        1: 11
        2: 10
        3: 7

    Columns (scanned OUTPUT):
        0: 6
        1: 5
        2: 4
*/
#define BTN_ROWS 4
#define BTN_ROW0 12
#define BTN_ROW1 11
#define BTN_ROW2 10
#define BTN_ROW3 7

#define BTN_COLS 3
#define BTN_COL0 6
#define BTN_COL1 5
#define BTN_COL2 4

#define NUM_BUTTONS (BTN_ROWS * BTN_COLS)

// PL9823 LEDs (WS2812B-compatible, 3 per turnout × 12 turnouts = 36)
// Requires 330Ω series resistor on data line and 470-1000µF cap on LED power
#define NUM_LEDS 36
#define LED_PIN 9

// Turnout control pins
// - 12 turnouts, each with 2 control pins, for a total of 24 pins needed
// - Each turnout is 2 pins going from 22-45
#define T0_IN1  22
#define T0_IN2  23
#define T1_IN1  24
#define T1_IN2  25
#define T2_IN1  26
#define T2_IN2  27
#define T3_IN1  28
#define T3_IN2  29
#define T4_IN1  30
#define T4_IN2  31
#define T5_IN1  34
#define T5_IN2  35
#define T6_IN1  36
#define T6_IN2  37
#define T7_IN1  32
#define T7_IN2  33
#define T8_IN1  42
#define T8_IN2  43
#define T9_IN1  38
#define T9_IN2  39
#define T10_IN1 40
#define T10_IN2 41
#define T11_IN1 44
#define T11_IN2 45  


// Rotary encoder
#define ENC_CLK 2
#define ENC_DT  3
#define ENC_SW  8

// LCD (TC1602A, 4-bit parallel)
#define LCD_RS 46
#define LCD_E  47
#define LCD_D4 48
#define LCD_D5 49
#define LCD_D6  50
#define LCD_D7  51

// I2C (DCC-EX communication)
// Pins 20/21 are the hardware I2C (TWI) pins on the ATmega2560.
// The hardware TWI operates as I2C slave at address 0x65 for DCC-EX.
#define I2C_SDA 20
#define I2C_SCL 21

// Software I2C bus for FRAM (MB85RC256V) — pins 14/15 are bit-banged via
// SoftwareWire. This is a private, master-only bus; the hardware TWI bus is
// reserved for the DCC-EX slave role and cannot be shared.
#define FRAM_SDA 14
#define FRAM_SCL 15

#endif // PINLAYOUT_H
