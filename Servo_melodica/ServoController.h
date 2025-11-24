#ifndef SERVOCONTROLLER_H
#define SERVOCONTROLLER_H
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include "settings.h"

class ServoController {
private:
  Adafruit_PWMServoDriver pwm1;
  Adafruit_PWMServoDriver pwm2;
  void setServoAngle(uint8_t servoNum, uint16_t angle);
  void resetServosPosition();// utilisé au demarrage pour deplacer les servos en position init-angle

public:
  ServoController(); //initialise toutles servomoteurs a l'angle de depart
  void noteOff(uint8_t servoNum); // met le servo a l'angle d'initilisation contre la touche 
  void noteOn(uint8_t servoNum);// actionne la touche avec le servo
};

#endif // SERVOCONTROLLER_H

