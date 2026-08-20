
#include <Arduino.h>

#define LED_PIN PC13

void setup()
{
  pinMode(LED_PIN, OUTPUT);
  Serial.begin(115200);
}

unsigned long prevMillis = 0;
const int interval = 1000;

void loop()
{
<<<<<<< HEAD
  unsigned long curMillis = millis();
=======
  unsigned long curMillis = millis(); 
>>>>>>> body

  while (curMillis - prevMillis >= interval)
  {
    prevMillis = curMillis;
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
}