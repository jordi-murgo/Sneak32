#ifndef LED_MANAGER_H
#define LED_MANAGER_H

#include <Arduino.h>

#ifdef ENABLE_LED
#include <Adafruit_NeoPixel.h>
#endif

class LedManager {
public:
    LedManager(uint8_t pin, uint16_t numPixels);
    void begin();
    void setPixelColor(uint16_t n, uint32_t color);
    void show();
    void clear();
    uint32_t Color(uint8_t r, uint8_t g, uint8_t b);

    static const uint32_t COLOR_OFF = 0x000000;
    static const uint32_t COLOR_RED = 0xFF0000;
    static const uint32_t COLOR_GREEN = 0x00FF00;
    static const uint32_t COLOR_BLUE = 0x0000FF;

private:
#ifdef ENABLE_LED
    Adafruit_NeoPixel pixels;
#endif
};

#endif // LED_MANAGER_H
