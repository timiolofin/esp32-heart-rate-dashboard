#ifndef ALGORITHM_BY_RF_H_
#define ALGORITHM_BY_RF_H_
#include <Arduino.h>

#define ST 4
#define FS 25

const float sum_X2 = 83325;
#define MAX_HR 180
#define MIN_HR 40

const float min_autocorrelation_ratio = 0.5;
const float min_pearson_correlation = 0.8;

const int32_t BUFFER_SIZE = FS * ST;
const int32_t FS60 = FS * 60;
const int32_t LOWEST_PERIOD = FS60 / MAX_HR;
const int32_t HIGHEST_PERIOD = FS60 / MIN_HR;
const float mean_X = (float)(BUFFER_SIZE - 1) / 2.0;

void rf_heart_rate_and_oxygen_saturation(
  uint32_t *pun_ir_buffer,
  int32_t n_ir_buffer_length,
  uint32_t *pun_red_buffer,
  float *pn_spo2,
  int8_t *pch_spo2_valid,
  int32_t *pn_heart_rate,
  int8_t *pch_hr_valid,
  float *ratio,
  float *correl
);

float rf_linear_regression_beta(float *pn_x, float xmean, float sum_x2);
float rf_autocorrelation(float *pn_x, int32_t n_size, int32_t n_lag);
float rf_rms(float *pn_x, int32_t n_size, float *sumsq);
float rf_Pcorrelation(float *pn_x, float *pn_y, int32_t n_size);
void rf_initialize_periodicity_search(float *pn_x, int32_t n_size, int32_t *p_last_periodicity, int32_t n_max_distance, float min_aut_ratio, float aut_lag0);
void rf_signal_periodicity(float *pn_x, int32_t n_size, int32_t *p_last_periodicity, int32_t n_min_distance, int32_t n_max_distance, float min_aut_ratio, float aut_lag0, float *ratio);

#endif