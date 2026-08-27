
#include <Arduino.h>
#include <FastLED.h>

#define NUM_LEDS 1
#define DATA_PIN 48

int i = 0;

CRGB leds[NUM_LEDS];

void setup()
{
    FastLED.addLeds<SK6812, DATA_PIN, GRB>(leds, NUM_LEDS);
    Serial.begin(115200);
}

unsigned long prevMillis = 0;
const int interval = 1000;

int color = 0;

void loop()
{
    unsigned long currMillis = millis();

    if (currMillis - prevMillis >= interval)
    {
        prevMillis = currMillis;

        leds[0] = CRGB::Black;
        FastLED.show();
    }
}