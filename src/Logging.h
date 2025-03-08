/**
 * @file Logging.h
 * @brief Logging utilities for the Sneak32 project
 */

#ifndef LOGGING_H
#define LOGGING_H

#include <Arduino.h>

// Custom logging macros for Sneak32
// We'll use the existing ESP32 logging macros instead of redefining them
// To use these in your code, include this header and use:
// log_e("Error message: %s", error_string);
// log_w("Warning message");
// log_i("Info message");
// log_d("Debug message");
// log_v("Verbose message");

// For ISR-safe logging, use the existing isr_log_* macros:
// isr_log_e("Error in ISR");
// isr_log_w("Warning in ISR");
// isr_log_i("Info in ISR");

#endif // LOGGING_H
