# I2C Bus Debugging with Rigol DS1054Z

Instructions for using the Rigol DS1054Z oscilloscope to debug I2C communication between the EX-CSB1 and the Arduino Mega Switching Controller.

*Reference: DS1000Z_UserGuide_EN.pdf, pages 79 (Edge Trigger), 106-107 (I2C Trigger), 176-178 (I2C Decoding)*

---

## Physical Setup

1. Connect **CH1 probe** to **SCL** on the HV (5V) side of the level shifter (the wire going to Mega pin 21)
2. Connect **CH2 probe** to **SDA** on the HV (5V) side of the level shifter (the wire going to Mega pin 20)
3. Connect **both ground clips** to the common GND

---

## Step 1: Enable Channels

1. Press **CH1** button on front panel to enable it (light turns on)
2. Turn **VERTICAL SCALE** knob to set **2V/div**
3. Press **CH2** button to select it
4. Turn **VERTICAL SCALE** knob to set **2V/div**

## Step 2: Set Horizontal Timebase

1. Turn the **HORIZONTAL SCALE** knob to set **50µs/div**

## Step 3: Set Up I2C Trigger

1. Press **MENU** in the **TRIGGER** control area
2. Press **Type** softkey, rotate the multifunction knob to select **I2C**, press the knob down to confirm
3. Press **SCL** softkey and select **CH1**
4. Press **SDA** softkey and select **CH2**
5. Press **When** softkey and select **Start** (triggers on I2C start condition)
6. Press **Sweep** softkey and select **Normal** (only triggers on actual I2C traffic, won't auto-trigger on noise)
7. Adjust **TRIGGER LEVEL** knob to approximately **2.5V**

## Step 4: Set Up I2C Protocol Decode

1. Press **MATH** button on front panel
2. Press **Decode1** softkey
3. Press **Decoder** softkey, rotate knob to select **I2C**, press down
4. Press **Decode** softkey to set it to **ON**
5. Press **CLK** softkey and select **CH1**
6. Press **DATA** softkey and select **CH2**

This will show decoded I2C addresses and data values directly on the waveform display.

## Step 5: Test

1. Press **RUN/STOP** to start acquisition (should show "Trig'd" or "WAIT" at top of screen)
2. Press the **Reset button** on the EX-CSB1

### Interpreting Results

**If the scope triggers (signals are present):**

The scope will capture and display SCL clock pulses on CH1 and SDA data on CH2. The I2C decoder will overlay address bytes on the waveform — look for:

- `0x3C` — the OLED display (confirms the bus is working)
- `0x65` — the Switching Controller address

If you see `0x3C` but not `0x65`, the EX-CSB1 is scanning the bus but the Mega is not responding at its address.

**If no trigger occurs ("WAIT" stays on screen):**

No I2C signals are reaching the probes. Move the probes to the **LV (3.3V) side** of the level shifter (directly at the EX-CSB1 header pins), and adjust settings:

1. Change **VERTICAL SCALE** to **1V/div** on both channels (3.3V signals are smaller)
2. Change **TRIGGER LEVEL** to approximately **1.5V**
3. Press **RUN/STOP** to restart acquisition
4. Press **Reset** on the EX-CSB1 again

If signals appear on the LV side but not the HV side, **the level shifter is the problem** — it is not passing signals through to the Mega side.

If no signals appear on either side, the EX-CSB1 is not generating I2C traffic on the Dual I2C Header. Check wiring to the correct header pins.
