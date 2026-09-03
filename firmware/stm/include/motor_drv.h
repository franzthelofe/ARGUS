
#ifndef MOTOR_DRV_H
#define MOTOR_DRV_H

// MOTOR 1 DRIVER PINS
#define IN1_PIN PA8
#define IN2_PIN PA9
#define ENA_PIN PB3

// MOTOR 2 DRIVER PINS
#define IN3_PIN PA0
#define IN4_PIN PA1
#define ENB_PIN PB4

void motorSetupPin(void);
    
void motorForward();
void motorBackward();
void motorStop();
void motorTurnLeft();
void motorTurnRight();

#endif // MOTOR_DRV_H