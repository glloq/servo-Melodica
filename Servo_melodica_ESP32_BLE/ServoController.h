#ifndef SERVOCONTROLLER_H
#define SERVOCONTROLLER_H
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include "settings.h"

class ServoController {
private:
  Adafruit_PWMServoDriver pwm1;
  Adafruit_PWMServoDriver pwm2;
  bool isInitialized;
  uint16_t currentAngles[NUMBER_OF_NOTES];     // Current servo angles
  int8_t currentDirections[NUMBER_OF_NOTES];   // Current servo directions
  void setServoAngle(uint8_t servoNum, uint16_t angle);
  void resetServosPosition();// utilisé au demarrage pour deplacer les servos en position init-angle

public:
  ServoController(); //initialise toutles servomoteurs a l'angle de depart
  bool begin(); // Initialize PWM drivers, returns true on success
  bool isReady(); // Check if controllers are properly initialized
  void noteOff(uint8_t servoNum); // Relâche la touche (position repos)
  void noteOn(uint8_t servoNum);  // Appuie sur la touche (position fixe)
};

#endif // SERVOCONTROLLER_H

