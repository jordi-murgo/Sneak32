#ifndef USER_SETUP_H
#define USER_SETUP_H

// TFT_eSPI configuration for ESP32-S3 LCD displays
// This file provides configuration for common ESP32-S3 LCD boards

#ifdef ENABLE_DISPLAY

// Driver selection - use build flags to define specific driver
// Example: -DILI9488_DRIVER=1

// Pin configuration - use build flags for hardware-specific pins
// Example: -DTFT_MISO=12 -DTFT_MOSI=11 etc.

#ifndef TFT_MISO
  #define TFT_MISO 12   // Default pins if not defined in build flags
#endif
#ifndef TFT_MOSI
  #define TFT_MOSI 11
#endif
#ifndef TFT_SCLK
  #define TFT_SCLK 14
#endif
#ifndef TFT_CS
  #define TFT_CS   10
#endif
#ifndef TFT_DC
  #define TFT_DC   13
#endif
#ifndef TFT_RST
  #define TFT_RST  21
#endif

// Backlight control (if available)
#ifndef TFT_BL
  #define TFT_BL   38  // LED back-light control pin
#endif
#ifndef TFT_BACKLIGHT_ON
  #define TFT_BACKLIGHT_ON HIGH  // Level to turn ON back-light (HIGH or LOW)
#endif

// Touch controller (if available)
#ifndef TOUCH_CS
  #define TOUCH_CS 33
#endif

// SPI frequency
#ifndef SPI_FREQUENCY
  #define SPI_FREQUENCY  27000000
#endif
#ifndef SPI_READ_FREQUENCY
  #define SPI_READ_FREQUENCY  20000000
#endif
#ifndef SPI_TOUCH_FREQUENCY
  #define SPI_TOUCH_FREQUENCY  2500000
#endif

// Color order
#ifndef TFT_RGB_ORDER
  #define TFT_RGB_ORDER TFT_BGR  // Colour order Blue-Green-Red
#endif

#endif // ENABLE_DISPLAY

#endif // USER_SETUP_H