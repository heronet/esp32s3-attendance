#include "indicators.h"
#include "ble_manager.h"

// Globals
Adafruit_NeoPixel pixels(NUM_PIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

void setupRGB()
{
    // Initialize NeoPixel
    pixels.begin();
    pixels.setBrightness(50); // Set brightness (0-255)

    // Quick test flash - red then green
    pixels.setPixelColor(0, pixels.Color(0, 255, 0)); // Green
    pixels.show();
    delay(300);
    pixels.setPixelColor(0, pixels.Color(255, 0, 0)); // Red
    pixels.show();
    delay(300);
    pixels.setPixelColor(0, pixels.Color(0, 0, 0)); // Turn off
    pixels.show();

    printBoth("NeoPixel LED initialized");
}

void indicateSuccess()
{
    pixels.setPixelColor(0, pixels.Color(0, 255, 0)); // Green
    pixels.show();
    delay(1000);
    pixels.setPixelColor(0, pixels.Color(0, 0, 0)); // Turn off
    pixels.show();
}

void indicateFailure()
{
    pixels.setPixelColor(0, pixels.Color(255, 0, 0)); // Red
    pixels.show();
    delay(1000);
    pixels.setPixelColor(0, pixels.Color(0, 0, 0)); // Turn off
    pixels.show();
}