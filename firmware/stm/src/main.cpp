// main.cpp


#include "motor_drv.h"

#include <Arduino.h>

#define LED_PIN PC13 

void setup()
{
  pinMode(LED_PIN, OUTPUT);

  digitalWrite(LED_PIN, HIGH);


  motorSetupPin();

  Serial.begin(115200);

}

void loop()
{
  static unsigned long prevMillis = 0;
  if (millis() - prevMillis >= 1000)
  {
    prevMillis = millis();
  }
  
}