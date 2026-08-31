// main.cpp

#include "fLed.h"
#include "wifi_hdl.h"

#include <Arduino.h>
#include <FastLED.h>

#define WIFI_SSID     "ARGUS Controller"
#define WIFI_PASSWORD "argus1.0"

unsigned long prevMillis = 0;
const int interval = 1000;

void setup()
{
    Serial.begin(115200);

    FastLED.addLeds<SK6812, DATA_PIN, GRB>(led, NUM_LED);

    setupWIFI(WIFI_SSID, WIFI_PASSWORD);
}

void loop()
{
     server.handleClient();
     
    unsigned long currMillis = millis();

    if (currMillis - prevMillis >= interval)
    {
        prevMillis = currMillis;

        
        FastLED.show();
        Serial.println("TIME: "); Serial.print((prevMillis/1000));
    }
}