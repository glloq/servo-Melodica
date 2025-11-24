#ifndef AUDIOCALIBRATION_H
#define AUDIOCALIBRATION_H

#include "settings.h"
#include "ServoController.h"
#include "Instrument.h"

/***********************************************************************************************
----------------------------    AudioCalibration.h   ----------------------------------------
************************************************************************************************

Classe pour la calibration automatique des servos par détection audio

Utilise un microphone pour détecter l'intensité sonore produite par chaque touche
du mélodica et détermine automatiquement l'angle optimal de chaque servo.

Principe :
1. Pour chaque servo, tester plusieurs angles (ex: 70°, 75°, 80°, ... 110°)
2. Mesurer l'intensité sonore produite à chaque angle
3. Sélectionner l'angle produisant le son le plus fort
4. Sauvegarder la calibration en EEPROM

************************************************************************************************/

class AudioCalibration {
private:
  ServoController& servoController;
  Instrument& instrument;
  uint16_t readSoundLevel();  // Lit le niveau sonore du microphone
  uint16_t readAverageSoundLevel(uint8_t samples);  // Moyenne sur plusieurs échantillons
  bool waitForSoundStabilization();  // Attend que le son se stabilise

public:
  AudioCalibration(ServoController& sc, Instrument& inst);

  // Calibration d'un seul servo
  bool calibrateServo(uint8_t servoNum);

  // Calibration de tous les servos
  void calibrateAllServos();

  // Test manuel d'un angle pour un servo
  uint16_t testServoAngle(uint8_t servoNum, uint16_t angle);

  // Vérifier si le microphone fonctionne
  bool checkMicrophone();
};

#endif // AUDIOCALIBRATION_H
