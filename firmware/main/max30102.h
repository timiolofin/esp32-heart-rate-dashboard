// =============================================================================
// max30102.h
// Header file for the MAX30102 optical pulse sensor driver.
// The MAX30102 communicates with the ESP32-S3 over I2C.
// It measures red and IR light absorption through the fingertip to compute
// heart rate and oxygen saturation (SpO2).
// =============================================================================

#ifndef MAX30102_H_
#define MAX30102_H_

#include <Arduino.h>

// I2C address — MAX30102 uses 0x57 for both read and write
#define I2C_WRITE_ADDR 0x57
#define I2C_READ_ADDR  0x57

// MAX30102 register map
#define REG_INTR_STATUS_1   0x00  // Interrupt status register 1
#define REG_INTR_STATUS_2   0x01  // Interrupt status register 2
#define REG_INTR_ENABLE_1   0x02  // Interrupt enable register 1
#define REG_INTR_ENABLE_2   0x03  // Interrupt enable register 2
#define REG_FIFO_WR_PTR     0x04  // FIFO write pointer
#define REG_OVF_COUNTER     0x05  // FIFO overflow counter
#define REG_FIFO_RD_PTR     0x06  // FIFO read pointer
#define REG_FIFO_DATA       0x07  // FIFO data output register
#define REG_FIFO_CONFIG     0x08  // FIFO configuration
#define REG_MODE_CONFIG     0x09  // Operating mode configuration
#define REG_SPO2_CONFIG     0x0A  // SpO2 sensor configuration
#define REG_LED1_PA         0x0C  // Red LED pulse amplitude
#define REG_LED2_PA         0x0D  // IR LED pulse amplitude
#define REG_PILOT_PA        0x10  // Proximity mode LED pulse amplitude
#define REG_MULTI_LED_CTRL1 0x11  // Multi-LED mode control register 1
#define REG_MULTI_LED_CTRL2 0x12  // Multi-LED mode control register 2
#define REG_TEMP_INTR       0x1F  // Die temperature integer
#define REG_TEMP_FRAC       0x20  // Die temperature fraction
#define REG_TEMP_CONFIG     0x21  // Die temperature config
#define REG_PROX_INT_THRESH 0x30  // Proximity interrupt threshold
#define REG_REV_ID          0xFE  // Revision ID
#define REG_PART_ID         0xFF  // Part ID — should read 0x15 for MAX30102

// Function declarations
bool maxim_max30102_init(int sda_pin, int scl_pin);               // Initialize sensor with I2C pins
bool maxim_max30102_read_fifo(uint32_t *pun_red_led, uint32_t *pun_ir_led); // Read one sample from FIFO
bool maxim_max30102_write_reg(uint8_t uch_addr, uint8_t uch_data);           // Write to a register
bool maxim_max30102_read_reg(uint8_t uch_addr, uint8_t *puch_data);          // Read from a register
bool maxim_max30102_reset(void);                                              // Soft reset the sensor
bool maxim_max30102_read_temperature(int8_t *integer_part, uint8_t *fractional_part); // Read die temp

#endif