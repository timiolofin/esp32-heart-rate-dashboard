// =============================================================================
// median_filter.h
// Fixed-size circular median filter template.
// Stores the last N values of any numeric type and computes the median on
// demand. An invalid sentinel value marks unfilled slots — those slots are
// excluded from the median so early readings don't bias the output.
//
// Used in main.ino to smooth HR and SpO2 across 4 algorithm cycles before
// the values are debounced and displayed.
// =============================================================================

#ifndef MEDIAN_FILTER_H_
#define MEDIAN_FILTER_H_

#include <algorithm>
#include <stddef.h>

// T — numeric type (e.g. int32_t for HR, float for SpO2)
// N — number of history slots (set by MEDIAN_HISTORY in main.ino)
template <typename T, size_t N>
class MedianFilter {
 public:
  // Initialise all slots to the invalid sentinel so the filter knows
  // which positions have not yet received a real reading.
  explicit MedianFilter(T invalid_value)
      : invalid_value_(invalid_value), write_index_(0) {
    std::fill(values_, values_ + N, invalid_value_);
  }

  // Write a new value into the next slot, overwriting the oldest entry.
  // Pass INVALID_HR / INVALID_SPO2 to explicitly mark a slot as empty
  // (used during reset to flush stale readings from the filter).
  void push(T value) {
    values_[write_index_] = value;
    write_index_ = (write_index_ + 1) % N;
  }

  // Compute the median of all non-invalid slots and write it to median_out.
  // Returns true if at least one valid slot exists, false if all are invalid.
  // For an even number of valid values, returns the average of the two middle
  // values to avoid bias toward either side.
  bool median(T* median_out) const {
    if (median_out == nullptr) return false;

    // Collect only the valid (non-sentinel) values
    T valid_values[N];
    size_t valid_count = 0;
    for (size_t i = 0; i < N; ++i) {
      T value = values_[i];
      if (value != invalid_value_) valid_values[valid_count++] = value;
    }

    if (valid_count == 0) return false;

    std::sort(valid_values, valid_values + valid_count);
    if (valid_count % 2) {
      *median_out = valid_values[valid_count / 2];
    } else {
      *median_out =
          (valid_values[valid_count / 2 - 1] + valid_values[valid_count / 2]) /
          static_cast<T>(2);
    }
    return true;
  }

 private:
  T      values_[N] = {};
  T      invalid_value_;
  size_t write_index_;
};

#endif  // MEDIAN_FILTER_H_