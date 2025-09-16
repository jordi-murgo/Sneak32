#ifndef USER_SETUP_H
#define USER_SETUP_H

// TFT_eSPI configuration for ESP32-S3 LCD displays
// This file provides configuration for common ESP32-S3 LCD boards

#ifdef ENABLE_DISPLAY

// Driver selection
#define ILI9488_DRIVER
// Alternative drivers for different displays:
// #define ST7796_DRIVER
// #define ILI9341_DRIVER

// ESP32-S3 pin configuration (adjust for your specific board)
#define TFT_MISO 12
#define TFT_MOSI 11
#define TFT_SCLK 14
#define TFT_CS   10
#define TFT_DC   13
#define TFT_RST  21  // Connect to ESP32 EN pin if not defined

// Backlight control (if available)
#define TFT_BL   38  // LED back-light control pin
#define TFT_BACKLIGHT_ON HIGH  // Level to turn ON back-light (HIGH or LOW)

// Touch controller (if available)
#define TOUCH_CS 33

// SPI frequency
#define SPI_FREQUENCY  27000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000

// Color order
#define TFT_RGB_ORDER TFT_BGR  // Colour order Blue-Green-Red
// Alternative: #define TFT_RGB_ORDER TFT_RGB

#endif // ENABLE_DISPLAY

#endif // USER_SETUP_H