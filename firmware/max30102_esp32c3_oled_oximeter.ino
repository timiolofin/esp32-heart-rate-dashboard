#include <Arduino.h>
#include <U8g2lib.h>
#include "algorithm_by_RF.h"
#include "max30102.h"
#include "algorithm.h" 
#include "median_filter.h"

// Interrupt pin
const byte oxiInt = 2; // pin connected to MAX30102 INT
const int SENSOR_I2C_SDA_PIN = 5;
const int SENSOR_I2C_SCL_PIN = 6;
const int HEARTBEAT_LED_PIN = 8;
const uint32_t HEARTBEAT_LED_PULSE_MS = 60;
const bool HEARTBEAT_LED_ACTIVE_LOW = true;

uint32_t elapsedTime,timeStart;

uint32_t aun_ir_buffer[BUFFER_SIZE]; // Infrared ring buffer.
uint32_t aun_red_buffer[BUFFER_SIZE];  // Red ring buffer.
uint32_t ordered_ir_buffer[BUFFER_SIZE]; // Oldest-to-newest IR view for algorithms.
uint32_t ordered_red_buffer[BUFFER_SIZE]; // Oldest-to-newest RED view for algorithms.
const int32_t UPDATE_STEP = 10; // New samples per update; keep <= BUFFER_SIZE.
const uint8_t MEDIAN_HISTORY = 4; // Number of previous samples kept for median filtering.
const int32_t INVALID_HR = -999;
const float INVALID_SPO2 = -999.0f;
uint16_t sample_write_index = 0;
uint16_t sample_count = 0;

MedianFilter<int32_t, MEDIAN_HISTORY> hr_filter(INVALID_HR);
MedianFilter<float, MEDIAN_HISTORY> spo2_filter(INVALID_SPO2);
uint32_t next_heartbeat_flash_ms = 0;
uint32_t heartbeat_led_off_ms = 0;
bool heartbeat_led_on = false;

static void setHeartbeatLed(bool on) {
  const int level = (on ^ HEARTBEAT_LED_ACTIVE_LOW) ? HIGH : LOW;
  digitalWrite(HEARTBEAT_LED_PIN, level);
}

static void waitForDataReady(uint32_t timeout_us) {
  uint32_t wait_start = micros();
  while(digitalRead(oxiInt)==HIGH) {
    if((micros()-wait_start) > timeout_us) {
      Serial.println("Given up on IR");
      break;
    }
    delayMicroseconds(50);
  }
}

static void readNextSampleIntoRing(uint32_t timeout_us) {
  waitForDataReady(timeout_us);
  maxim_max30102_read_fifo(&aun_red_buffer[sample_write_index], &aun_ir_buffer[sample_write_index]);
  sample_write_index = (sample_write_index + 1) % BUFFER_SIZE;
  if(sample_count < BUFFER_SIZE) ++sample_count;
}

static void buildOrderedWindow() {
  // Next write index always points at the oldest sample in a full ring.
  uint16_t start = sample_write_index;
  for(uint16_t i=0; i<BUFFER_SIZE; ++i) {
    uint16_t src = (start + i) % BUFFER_SIZE;
    ordered_red_buffer[i] = aun_red_buffer[src];
    ordered_ir_buffer[i] = aun_ir_buffer[src];
  }
}

static void updateHeartbeatLed(int32_t hr_bpm) {
  const uint32_t now_ms = millis();

  if(heartbeat_led_on && now_ms >= heartbeat_led_off_ms) {
    setHeartbeatLed(false);
    heartbeat_led_on = false;
  }

  // Keep blink output tied to plausible heart-rate values.
  if(hr_bpm < 35 || hr_bpm > 220) {
    next_heartbeat_flash_ms = 0;
    return;
  }

  const uint32_t beat_period_ms = 60000UL / (uint32_t)hr_bpm;
  if(next_heartbeat_flash_ms == 0) {
    next_heartbeat_flash_ms = now_ms;
  }

  if(now_ms >= next_heartbeat_flash_ms) {
    setHeartbeatLed(true);
    heartbeat_led_on = true;
    heartbeat_led_off_ms = now_ms + HEARTBEAT_LED_PULSE_MS;
    next_heartbeat_flash_ms = now_ms + beat_period_ms;
  }
}

class U8G2_SSD1306_72X40_NONAME_F_HW_I2C : public U8G2 {
  public: U8G2_SSD1306_72X40_NONAME_F_HW_I2C(const u8g2_cb_t *rotation, uint8_t reset = U8X8_PIN_NONE, uint8_t clock = U8X8_PIN_NONE, uint8_t data = U8X8_PIN_NONE) : U8G2() {
    u8g2_Setup_ssd1306_i2c_72x40_er_f(&u8g2, rotation, u8x8_byte_arduino_hw_i2c, u8x8_gpio_and_delay_arduino);
    u8x8_SetPin_HW_I2C(getU8x8(), reset, clock, data);
  }
};

U8G2_SSD1306_72X40_NONAME_F_HW_I2C u8g2(U8G2_R2, U8X8_PIN_NONE, SENSOR_I2C_SCL_PIN, SENSOR_I2C_SDA_PIN);

void setup() {
  pinMode(oxiInt, INPUT);
  pinMode(HEARTBEAT_LED_PIN, OUTPUT);
  setHeartbeatLed(false);
  Serial.begin(115200);

  maxim_max30102_init(SENSOR_I2C_SDA_PIN, SENSOR_I2C_SCL_PIN);  //initialize the MAX30102

  u8g2.begin();

  Serial.print(F("Time[s]\tSpO2\tHR\tSpO2_MX\tHR_MX\tClock\tRatio\tCorr\tTemp[C]"));
  Serial.println("");

  timeStart=millis();
}

static void updateDisplay(int32_t hr, int32_t spo2) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_10x20_mr);
  u8g2.setCursor(0, 19);
  if (hr >= 30 && hr <= 250) {
    u8g2.printf("HR: %d", (int)hr);
  } else {
    u8g2.print("HR: --");
  }
  u8g2.setCursor(0, 39);
  if (spo2 >= 0 && spo2 <= 100) {
    u8g2.printf("O2: %d%%", (int)spo2);
  } else {
    u8g2.print("O2: --");
  }
  u8g2.sendBuffer();
}


//Continuously taking samples from MAX30102.  Heart rate and SpO2 are calculated every ST seconds
void loop() {
  float n_spo2,ratio,correl;  //SPO2 value
  int8_t ch_spo2_valid;  //indicator to show if the SPO2 calculation is valid
  int32_t n_heart_rate; //heart rate value
  int8_t  ch_hr_valid;  //indicator to show if the heart rate calculation is valid
  int32_t filtered_heart_rate;
  float filtered_spo2;
  char hr_str[10];
  const uint32_t data_ready_timeout_us = (2000000UL / FS); // Allow up to 2 sample periods.

  for(int32_t n=0; n<UPDATE_STEP; ++n) {
    readNextSampleIntoRing(data_ready_timeout_us);
  }

  // Algorithm expects full chronological window.
  if(sample_count < BUFFER_SIZE) return;
  buildOrderedWindow();

  //calculate heart rate and SpO2 after BUFFER_SIZE samples (ST seconds of samples) using Robert's method
  rf_heart_rate_and_oxygen_saturation(ordered_ir_buffer, BUFFER_SIZE, ordered_red_buffer, &n_spo2, &ch_spo2_valid, &n_heart_rate, &ch_hr_valid, &ratio, &correl); 
  elapsedTime=millis()-timeStart;
  millis_to_hours(elapsedTime,hr_str); // Time in hh:mm:ss format
  elapsedTime/=1000; // Time in seconds

  // Read the _chip_ temperature in degrees Celsius
  int8_t integer_temperature;
  uint8_t fractional_temperature;
  maxim_max30102_read_temperature(&integer_temperature, &fractional_temperature);
  float temperature = integer_temperature + ((float)fractional_temperature)/16.0;

  //calculate heart rate and SpO2 after BUFFER_SIZE samples (ST seconds of samples) using MAXIM's method
  float n_spo2_maxim;  //SPO2 value
  int8_t ch_spo2_valid_maxim;  //indicator to show if the SPO2 calculation is valid
  int32_t n_heart_rate_maxim; //heart rate value
  int8_t  ch_hr_valid_maxim;  //indicator to show if the heart rate calculation is valid
  maxim_heart_rate_and_oxygen_saturation(ordered_ir_buffer, BUFFER_SIZE, ordered_red_buffer, &n_spo2_maxim, &ch_spo2_valid_maxim, &n_heart_rate_maxim, &ch_hr_valid_maxim); 

  filtered_heart_rate = n_heart_rate;
  filtered_spo2 = n_spo2;
  int32_t median_hr;
  float median_spo2;
  if(hr_filter.median(&median_hr)) filtered_heart_rate = median_hr;
  if(spo2_filter.median(&median_spo2)) filtered_spo2 = median_spo2;
  updateHeartbeatLed(filtered_heart_rate);

  //if(ch_hr_valid && ch_spo2_valid || ch_hr_valid_maxim && ch_spo2_valid_maxim) {
    Serial.print(elapsedTime);
    Serial.print("\t");
    Serial.print(filtered_spo2);
    Serial.print("\t");
    Serial.print(filtered_heart_rate, DEC);
    Serial.print("\t");
    Serial.print(n_spo2_maxim);
    Serial.print("\t");
    Serial.print(n_heart_rate_maxim, DEC);
    Serial.print("\t");
    Serial.print(hr_str);
    Serial.print("\t");
    Serial.print(ratio);
    Serial.print("\t");
    Serial.print(correl);
    Serial.print("\t");
    Serial.print(temperature);
    Serial.println("");

    updateDisplay(filtered_heart_rate, (int32_t)filtered_spo2);
  //}

  hr_filter.push(ch_hr_valid ? n_heart_rate : INVALID_HR);
  spo2_filter.push(ch_spo2_valid ? n_spo2 : INVALID_SPO2);
}

void millis_to_hours(uint32_t ms, char* hr_str)
{
  char istr[6];
  uint32_t secs,mins,hrs;
  secs=ms/1000; // time in seconds
  mins=secs/60; // time in minutes
  secs-=60*mins; // leftover seconds
  hrs=mins/60; // time in hours
  mins-=60*hrs; // leftover minutes
  itoa(hrs,hr_str,10);
  strcat(hr_str,":");
  itoa(mins,istr,10);
  strcat(hr_str,istr);
  strcat(hr_str,":");
  itoa(secs,istr,10);
  strcat(hr_str,istr);
}

