#ifndef INDICATORS_H
#define INDICATORS_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "config.h"

// Globals
extern Adafruit_NeoPixel pixels;

// Function prototypes
void setupRGB();
void indicateSuccess();
void indicateFailure();

#endif // INDICATORS_H