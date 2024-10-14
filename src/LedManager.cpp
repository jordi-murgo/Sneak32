#include "LedManager.h"

LedManager::LedManager(uint8_t pin, uint16_t numPixels) 
#ifdef ENABLE_LED
    : pixels(numPixels, pin, NEO_GRB + NEO_KHZ800)
#endif
{
}

void LedManager::begin() {
#ifdef ENABLE_LED
    pixels.begin();
    pixels.show(); // Inicializa todos los píxeles a "apagado"
#endif
}

void LedManager::setPixelColor(uint16_t n, uint32_t color) {
#ifdef ENABLE_LED
    pixels.setPixelColor(n, color);
#endif
}

void LedManager::show() {
#ifdef ENABLE_LED
    pixels.show();
#endif
}

void LedManager::clear() {
#ifdef ENABLE_LED
    pixels.clear();
#endif
}

uint32_t LedManager::Color(uint8_t r, uint8_t g, uint8_t b) {
#ifdef ENABLE_LED
    return pixels.Color(r, g, b);
#else
    return 0;
#endif
}
