
#ifndef FLED_H
#define FLED_H

#include <FastLED.h>

#define NUM_LED 1
#define DATA_PIN 48

extern CRGB led[NUM_LED];

typedef enum Color
{
    R1 , R2, R3, R4,
    G1 , G2, G3, G4,
    B1_, B2, B3, B4,
    OFF
} Color;

void LEDColor(enum Color c);

inline void LEDShow(void) { FastLED.show(); };

#endif //FLED_H