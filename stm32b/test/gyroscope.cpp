#include <Arduino.h>
#include <Wire.h>
#include <MPU6050.h>

MPU6050 mpu;

void setup() {
    Serial.begin(115200);
    delay(1500);
    Wire.begin();       // Diko kabalo sa exact Pins ani, Ikw lng set-up sa pins
    Serial.println("Starting MPU6050...");
    mpu.initialize();
    Serial.println("MPU6050 initialized.");
    if (!mpu.testConnection()) {
        Serial.println("MPU6050 connection failed!");
        while (1);
    }

    if (!mpu.testConnection()){
        Serial.println("MPU6050 connection successful!");
    } else {
        Serial.println("MPU6050 connection failed!");
        while (1);     

    }

    //  ikaw set ani choi
       
    

}

void loop() {
    int16_t gx, gy, gz;
    mpu.getRotation(&gx, &gy, &gz);
    Serial.print("gyroscope\tX:");
    Serial.print(gx);
    Serial.print("gyroscope\tY:");
    Serial.print(gy);
    Serial.print("gyroscope\tZ:");
    Serial.println(gz);

    delay(1000); // Delay for 1 second before the next reading

}
