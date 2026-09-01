
#include <Arduino.h>
#include <SoftwareSerial.h>

#define LED_PIN PC13
#define TXD_PIN PB8
#define RXD_PIN PB9
#define IN1_PIN PA0
#define IN2_PIN PA1

SoftwareSerial btSerial(RXD_PIN, TXD_PIN);

void setup()
{
  pinMode(LED_PIN, OUTPUT);
  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);
  btSerial.begin(9600);
  Serial.begin(115200);

}

void loop()
{
  static unsigned long prevMillis = 0;
  if (millis() - prevMillis >= 1000)
  {
    prevMillis = millis();
    btSerial.print("Uptime: ");
    btSerial.println(millis() / 1000);
  }

  if (btSerial.available())
  {
    char c = btSerial.read();
    
    if (c == '1')
    {
      digitalWrite(LED_PIN, LOW);
      digitalWrite(IN1_PIN, HIGH);
      digitalWrite(IN2_PIN, LOW);
    }

    if (c == '0')
    {
      digitalWrite(LED_PIN, HIGH);
      digitalWrite(IN1_PIN, LOW);
      digitalWrite(IN2_PIN, HIGH);
    }
  }
}