// fLed.cpp

#include "fLed.h"
#include <FastLED.h>

CRGB led[NUM_LED];

void LEDColor(enum Color c)
{
    switch (c)
    {
        case R1:
            led[0] = CRGB(255, 0, 0);
            break;
        case R2:
            led[0] = CRGB(180, 0, 0);
            break;
        case R3:
            led[0] = CRGB(100, 0, 0);
            break;
        case R4:
            led[0] = CRGB(40, 0, 0);
            break;
        case G1:
            led[0] = CRGB(0, 255, 0);
            break;
        case G2:
            led[0] = CRGB(0, 180, 0);
            break;
        case G3:
            led[0] = CRGB(0, 100, 0);
            break;
        case G4:
            led[0] = CRGB(0, 40, 0);
            break;
        case B1_:
            led[0] = CRGB(0, 0, 255);
            break;
        case B2:
            led[0] = CRGB(0, 0, 180);
            break;
        case B3:
            led[0] = CRGB(0, 0, 100);
            break;
        case B4:
            led[0] = CRGB(0, 0, 40);
            break;

        case OFF:
            led[0] = CRGB(0, 0, 0);
            break;
    }
}