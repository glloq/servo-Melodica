#ifndef SERVOCONTROLLER_H
#define SERVOCONTROLLER_H
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <EEPROM.h>
#include "settings.h"

// Structure to store calibration data in EEPROM
struct CalibrationData {
  uint16_t magicNumber;       // Magic number for validation
  uint8_t version;            // Data structure version
  uint16_t servoAngles[NUMBER_OF_NOTES];  // Initial angles for each servo
  int8_t servoDirections[NUMBER_OF_NOTES]; // Rotation direction for each servo
  uint16_t checksum;          // Simple checksum for data integrity
};

class ServoController {
private:
  Adafruit_PWMServoDriver pwm1;
  Adafruit_PWMServoDriver pwm2;
  bool isInitialized;
  uint16_t currentAngles[NUMBER_OF_NOTES];     // Current servo angles
  int8_t currentDirections[NUMBER_OF_NOTES];   // Current servo directions
  void setServoAngle(uint8_t servoNum, uint16_t angle);
  void resetServosPosition();// utilisé au demarrage pour deplacer les servos en position init-angle
  uint16_t calculateChecksum(const CalibrationData& data);

public:
  ServoController(); //initialise toutles servomoteurs a l'angle de depart
  bool begin(); // Initialize PWM drivers, returns true on success
  bool isReady(); // Check if controllers are properly initialized
  void noteOff(uint8_t servoNum); // met le servo a l'angle d'initilisation contre la touche
  void noteOn(uint8_t servoNum, uint8_t velocity = 127);// actionne la touche avec le servo, velocity 0-127

  // Calibration functions
  bool saveCalibration(); // Save current calibration to EEPROM
  bool loadCalibration(); // Load calibration from EEPROM
  void setServoCalibration(uint8_t servoNum, uint16_t angle, int8_t direction);
  void resetToDefaultCalibration(); // Reset to factory defaults
  bool isCalibrationValid(); // Check if EEPROM contains valid data
};

#endif // SERVOCONTROLLER_H

