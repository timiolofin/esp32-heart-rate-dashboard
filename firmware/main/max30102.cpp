// =============================================================================
// max30102.cpp
// Driver implementation for the MAX30102 optical pulse sensor.
// Handles I2C communication, sensor initialization, and FIFO data reading.
// The sensor outputs red and IR channel values which are fed into the
// RF algorithm for heart rate and SpO2 estimation.
// =============================================================================

#include "max30102.h"
#include <Wire.h>

// Write a single byte to a MAX30102 register over I2C
bool maxim_max30102_write_reg(uint8_t uch_addr, uint8_t uch_data) {
  Wire.beginTransmission(I2C_WRITE_ADDR);
  Wire.write(uch_addr);   // Register address
  Wire.write(uch_data);   // Value to write
  Wire.endTransmission();
  return true;
}

// Read a single byte from a MAX30102 register over I2C
bool maxim_max30102_read_reg(uint8_t uch_addr, uint8_t *puch_data) {
  Wire.beginTransmission(I2C_WRITE_ADDR);
  Wire.write(uch_addr);   // Register address to read from
  Wire.endTransmission();

  Wire.beginTransmission(I2C_READ_ADDR);
  Wire.requestFrom(I2C_READ_ADDR, 1);
  *puch_data = Wire.read();
  Wire.endTransmission();
  return true;
}

// Soft reset the sensor by writing 0x40 to the mode config register
bool maxim_max30102_reset() {
  if (!maxim_max30102_write_reg(REG_MODE_CONFIG, 0x40)) {
    return false;
  }
  return true;
}

// =============================================================================
// maxim_max30102_init()
// Initializes the MAX30102 sensor over I2C and configures it for SpO2 mode.
// Must be called once in setup() before any readings are taken.
// SDA and SCL pins are configurable for ESP32 — uses GPIO5 and GPIO6.
// =============================================================================
bool maxim_max30102_init(int sda_pin, int scl_pin) {
#if defined(ARDUINO_ARCH_ESP32)
  Wire.begin(sda_pin, scl_pin);   // ESP32 allows custom I2C pins
#else
  (void)sda_pin;
  (void)scl_pin;
  Wire.begin();                   // Other platforms use default I2C pins
#endif

  Wire.setClock(400000L);         // Fast mode I2C — 400kHz

  maxim_max30102_reset();
  delay(1000);                    // Allow sensor to complete reset

  // Clear interrupt flags by reading status registers
  uint8_t uch_dummy;
  maxim_max30102_read_reg(REG_INTR_STATUS_1, &uch_dummy);

  // Configure interrupts, FIFO, mode, SpO2, and LED current
  if (!maxim_max30102_write_reg(REG_INTR_ENABLE_1, 0xc0)) return false;  // Enable FIFO almost full + new data interrupts
  if (!maxim_max30102_write_reg(REG_INTR_ENABLE_2, 0x00)) return false;  // Disable temp interrupt
  if (!maxim_max30102_write_reg(REG_FIFO_WR_PTR,   0x00)) return false;  // Reset FIFO write pointer
  if (!maxim_max30102_write_reg(REG_OVF_COUNTER,   0x00)) return false;  // Reset overflow counter
  if (!maxim_max30102_write_reg(REG_FIFO_RD_PTR,   0x00)) return false;  // Reset FIFO read pointer
  if (!maxim_max30102_write_reg(REG_FIFO_CONFIG,   0x0f)) return false;  // Sample averaging = 4, FIFO rollover off
  if (!maxim_max30102_write_reg(REG_MODE_CONFIG,   0x03)) return false;  // SpO2 mode (red + IR)
  if (!maxim_max30102_write_reg(REG_SPO2_CONFIG,   0x27)) return false;  // 100 Hz sample rate, 411us pulse width, 4096nA range
  if (!maxim_max30102_write_reg(REG_LED1_PA,       0x30)) return false;  // Red LED current ~7 mA
  if (!maxim_max30102_write_reg(REG_LED2_PA,       0x30)) return false;  // IR LED current ~7 mA
  if (!maxim_max30102_write_reg(REG_PILOT_PA,      0x7f)) return false;  // Pilot LED for proximity detection

  return true;
}

// =============================================================================
// maxim_max30102_read_fifo()
// Reads one sample (red + IR) from the sensor FIFO buffer.
// Each channel is 18-bit — stored across 3 bytes, masked to 0x03FFFF.
// Called in the main loop to continuously sample optical data.
// =============================================================================
bool maxim_max30102_read_fifo(uint32_t *pun_red_led, uint32_t *pun_ir_led) {
  uint32_t un_temp;
  uint8_t uch_temp;

  *pun_ir_led  = 0;
  *pun_red_led = 0;

  // Clear interrupt flags before reading
  maxim_max30102_read_reg(REG_INTR_STATUS_1, &uch_temp);
  maxim_max30102_read_reg(REG_INTR_STATUS_2, &uch_temp);

  // Point to the FIFO data register
  Wire.beginTransmission(I2C_WRITE_ADDR);
  Wire.write(REG_FIFO_DATA);
  Wire.endTransmission();

  // Read 6 bytes: 3 for red, 3 for IR
  Wire.beginTransmission(I2C_READ_ADDR);
  Wire.requestFrom(I2C_READ_ADDR, 6);

  // Reconstruct 18-bit red channel value from 3 bytes
  un_temp = Wire.read(); un_temp <<= 16; *pun_red_led += un_temp;
  un_temp = Wire.read(); un_temp <<= 8;  *pun_red_led += un_temp;
  un_temp = Wire.read();                 *pun_red_led += un_temp;

  // Reconstruct 18-bit IR channel value from 3 bytes
  un_temp = Wire.read(); un_temp <<= 16; *pun_ir_led += un_temp;
  un_temp = Wire.read(); un_temp <<= 8;  *pun_ir_led += un_temp;
  un_temp = Wire.read();                 *pun_ir_led += un_temp;

  Wire.endTransmission();

  // Mask to 18 bits
  *pun_red_led &= 0x03FFFF;
  *pun_ir_led  &= 0x03FFFF;

  return true;
}

// Read the sensor die temperature (integer + fractional parts)
bool maxim_max30102_read_temperature(int8_t *integer_part, uint8_t *fractional_part) {
  maxim_max30102_write_reg(REG_TEMP_CONFIG, 0x1);  // Trigger temperature measurement
  delayMicroseconds(1);

  uint8_t temp;
  maxim_max30102_read_reg(REG_TEMP_INTR, &temp);
  *integer_part = temp;
  maxim_max30102_read_reg(REG_TEMP_FRAC, fractional_part);

  return true;
}