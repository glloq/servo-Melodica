/***********************************************************************************************
----------------------------         SETTINGS               ------------------------------------
************************************************************************************************/
#ifndef SETTINGS_H
#define SETTINGS_H

#include "stdint.h"
#define DEBUG 0

// ------------------------------------------- MIDI -------------------------------
#define NUMBER_OF_NOTES 32
//note la plus grave du melodica
#define FIRST_MIDI_NOTE 65

//------------------------------------------- Air Manager -------------------------
// Servo d'air branché directement sur PWM Arduino
#define AIR_SERVO_PIN 9           // Pin PWM pour servo de valve d'air
#define AIR_CLOSED_ANGLE 0        // Angle lorsque valve fermée (pas d'air)
#define AIR_MIN_ANGLE 30          // Angle minimal pour notes douces
#define AIR_MAX_ANGLE 90          // Angle maximal pour notes fortes
#define AIR_ANTICIPATION_MS 50    // Délai d'anticipation avant noteOn (ms)


//------------------------------------------- Servos Manager -------------------------
// Configuration des 30 servos pour les touches du clavier
// Ordre : du plus grave (touche 0) au plus aigu (touche 29)

// Angles initiaux (position repos, touche relâchée) - À calibrer
const uint16_t initialAngles[NUMBER_OF_NOTES] {90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90};

// Sens de rotation pour chaque servo
// +1 = rotation horaire pour appuyer, -1 = rotation anti-horaire
// À ajuster selon le montage mécanique de chaque servo
const int8_t sensRot[NUMBER_OF_NOTES] {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};

// Angle de course pour appuyer sur les touches (identique pour tous)
#define ANGLE_NOTE_ON 20          // Déplacement en degrés pour appuyer
#define SERVO_RESET_DELAY_MS 200  // Délai entre chaque servo lors du reset

#define PCA1_ADRESS 0x40
#define PCA2_ADRESS 0x41
#define PWM_CHANNELS_PER_DRIVER 15  // Number of PWM channels per PCA9685

#define PIN_PCA_OFF 5// pin pour desactiver alim des servos et reduire le bruit

//reglages des PCA9685 pour des servo sg90
#define SERVO_MIN_ANGLE 0
#define SERVO_MAX_ANGLE 180
#define MICROSECONDS_PER_SECOND 1000000L  // For PWM calculations
const uint16_t SERVO_PULSE_MIN = 500;
const uint16_t SERVO_PULSE_MAX = 2500;
const uint16_t SERVO_FREQUENCY = 50;

#endif
