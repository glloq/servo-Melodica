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
#define PIN_VALVE1 6
#define PIN_VALVE2 4


//------------------------------------------- Servos Manager -------------------------
//ordre des servos dans l'ordre du plus grave au plus aigu
const uint16_t initialAngles[NUMBER_OF_NOTES] {90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90,90};
const uint16_t sensRot[NUMBER_OF_NOTES] {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};

#define ANGLE_NOTE_ON 20

#define PCA1_ADRESS 0x40
#define PCA2_ADRESS 0x41

#define PIN_PCA_OFF 5// pin pour desactiver alim des servos et reduire le bruit

//reglages des PCA9685 pour des servo sg90 
#define SERVO_MIN_ANGLE 0
#define SERVO_MAX_ANGLE 180
const uint16_t SERVO_PULSE_MIN = 500;
const uint16_t SERVO_PULSE_MAX = 2500;
const uint16_t SERVO_FREQUENCY = 50;

#endif
