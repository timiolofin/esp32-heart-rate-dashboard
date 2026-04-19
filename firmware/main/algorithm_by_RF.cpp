// =============================================================================
// algorithm_by_RF.h
// Header for the RF-style heart rate and SpO2 estimation algorithm.
// Processes buffered red and IR PPG signals from the MAX30102 to compute
// BPM using autocorrelation-based periodicity detection, and SpO2 using
// the red/IR ratio method.
//
// Algorithm parameters:
//   - Sample rate (FS): 25 Hz
//   - Buffer duration (ST): 4 seconds
//   - Buffer size: 100 samples (FS * ST)
//   - HR range: 40–180 BPM
// =============================================================================

#ifndef ALGORITHM_BY_RF_H_
#define ALGORITHM_BY_RF_H_
#include <Arduino.h>

#define ST 4    // Buffer duration in seconds
#define FS 25   // Sampling frequency in Hz

const float sum_X2 = 83325;   // Precomputed sum of squared x values for linear regression

#define MAX_HR 180   // Maximum detectable heart rate (BPM)
#define MIN_HR 40    // Minimum detectable heart rate (BPM)

// Minimum autocorrelation ratio required to accept a periodicity estimate
const float min_autocorrelation_ratio = 0.5;

// Minimum Pearson correlation between red and IR signals required for valid SpO2
const float min_pearson_correlation = 0.8;

// Derived constants
const int32_t BUFFER_SIZE   = FS * ST;          // 100 samples
const int32_t FS60          = FS * 60;           // Samples per minute — used to convert period to BPM
const int32_t LOWEST_PERIOD  = FS60 / MAX_HR;   // Shortest valid period (samples)
const int32_t HIGHEST_PERIOD = FS60 / MIN_HR;   // Longest valid period (samples)
const float   mean_X         = (float)(BUFFER_SIZE - 1) / 2.0;  // Mean of x indices for regression

// =============================================================================
// Main function — computes heart rate and SpO2 from buffered red/IR data
// Outputs: pn_heart_rate, pch_hr_valid, pn_spo2, pch_spo2_valid, ratio, correl
// =============================================================================
void rf_heart_rate_and_oxygen_saturation(
  uint32_t *pun_ir_buffer,        // IR channel sample buffer
  int32_t   n_ir_buffer_length,   // Number of samples in buffer
  uint32_t *pun_red_buffer,       // Red channel sample buffer
  float    *pn_spo2,              // Output: SpO2 percentage
  int8_t   *pch_spo2_valid,       // Output: 1 if SpO2 is valid
  int32_t  *pn_heart_rate,        // Output: heart rate in BPM
  int8_t   *pch_hr_valid,         // Output: 1 if heart rate is valid
  float    *ratio,                // Output: red/IR AC ratio
  float    *correl                // Output: Pearson correlation coefficient
);

// Internal signal processing helpers
float rf_linear_regression_beta(float *pn_x, float xmean, float sum_x2);
float rf_autocorrelation(float *pn_x, int32_t n_size, int32_t n_lag);
float rf_rms(float *pn_x, int32_t n_size, float *sumsq);
float rf_Pcorrelation(float *pn_x, float *pn_y, int32_t n_size);
void  rf_initialize_periodicity_search(float *pn_x, int32_t n_size, int32_t *p_last_periodicity, int32_t n_max_distance, float min_aut_ratio, float aut_lag0);
void  rf_signal_periodicity(float *pn_x, int32_t n_size, int32_t *p_last_periodicity, int32_t n_min_distance, int32_t n_max_distance, float min_aut_ratio, float aut_lag0, float *ratio);

#endif