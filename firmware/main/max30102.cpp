#include "max30102.h"
#include <Wire.h>

bool maxim_max30102_write_reg(uint8_t uch_addr, uint8_t uch_data) {
  Wire.beginTransmission(I2C_WRITE_ADDR);
  Wire.write(uch_addr);
  Wire.write(uch_data);
  Wire.endTransmission();
  return true;
}

bool maxim_max30102_read_reg(uint8_t uch_addr, uint8_t *puch_data) {
  Wire.beginTransmission(I2C_WRITE_ADDR);
  Wire.write(uch_addr);
  Wire.endTransmission();

  Wire.beginTransmission(I2C_READ_ADDR);
  Wire.requestFrom(I2C_READ_ADDR, 1);
  *puch_data = Wire.read();
  Wire.endTransmission();
  return true;
}

bool maxim_max30102_reset() {
  if (!maxim_max30102_write_reg(REG_MODE_CONFIG, 0x40)) {
    return false;
  }
  return true;
}

bool maxim_max30102_init(int sda_pin, int scl_pin) {
#if defined(ARDUINO_ARCH_ESP32)
  Wire.begin(sda_pin, scl_pin);
#else
  (void)sda_pin;
  (void)scl_pin;
  Wire.begin();
#endif

  Wire.setClock(400000L);

  maxim_max30102_reset();
  delay(1000);

  uint8_t uch_dummy;
  maxim_max30102_read_reg(REG_INTR_STATUS_1, &uch_dummy);

  if (!maxim_max30102_write_reg(REG_INTR_ENABLE_1, 0xc0)) return false;
  if (!maxim_max30102_write_reg(REG_INTR_ENABLE_2, 0x00)) return false;
  if (!maxim_max30102_write_reg(REG_FIFO_WR_PTR, 0x00)) return false;
  if (!maxim_max30102_write_reg(REG_OVF_COUNTER, 0x00)) return false;
  if (!maxim_max30102_write_reg(REG_FIFO_RD_PTR, 0x00)) return false;
  if (!maxim_max30102_write_reg(REG_FIFO_CONFIG, 0x0f)) return false;   // avg=4
  if (!maxim_max30102_write_reg(REG_MODE_CONFIG, 0x03)) return false;   // SpO2 mode
  if (!maxim_max30102_write_reg(REG_SPO2_CONFIG, 0x27)) return false;   // 100 Hz, 411 us, 4096 nA
  if (!maxim_max30102_write_reg(REG_LED1_PA, 0x30)) return false;       // ~7 mA red
  if (!maxim_max30102_write_reg(REG_LED2_PA, 0x30)) return false;       // ~7 mA IR
  if (!maxim_max30102_write_reg(REG_PILOT_PA, 0x7f)) return false;      // pilot

  return true;
}

bool maxim_max30102_read_fifo(uint32_t *pun_red_led, uint32_t *pun_ir_led) {
  uint32_t un_temp;
  uint8_t uch_temp;

  *pun_ir_led = 0;
  *pun_red_led = 0;

  maxim_max30102_read_reg(REG_INTR_STATUS_1, &uch_temp);
  maxim_max30102_read_reg(REG_INTR_STATUS_2, &uch_temp);

  Wire.beginTransmission(I2C_WRITE_ADDR);
  Wire.write(REG_FIFO_DATA);
  Wire.endTransmission();

  Wire.beginTransmission(I2C_READ_ADDR);
  Wire.requestFrom(I2C_READ_ADDR, 6);

  un_temp = Wire.read();
  un_temp <<= 16;
  *pun_red_led += un_temp;

  un_temp = Wire.read();
  un_temp <<= 8;
  *pun_red_led += un_temp;

  un_temp = Wire.read();
  *pun_red_led += un_temp;

  un_temp = Wire.read();
  un_temp <<= 16;
  *pun_ir_led += un_temp;

  un_temp = Wire.read();
  un_temp <<= 8;
  *pun_ir_led += un_temp;

  un_temp = Wire.read();
  *pun_ir_led += un_temp;

  Wire.endTransmission();

  *pun_red_led &= 0x03FFFF;
  *pun_ir_led &= 0x03FFFF;

  return true;
}

bool maxim_max30102_read_temperature(int8_t *integer_part, uint8_t *fractional_part) {
  maxim_max30102_write_reg(REG_TEMP_CONFIG, 0x1);
  delayMicroseconds(1);

  uint8_t temp;
  maxim_max30102_read_reg(REG_TEMP_INTR, &temp);
  *integer_part = temp;
  maxim_max30102_read_reg(REG_TEMP_FRAC, fractional_part);

  return true;
}