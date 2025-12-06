#ifndef INSTRUMENT_H
#define INSTRUMENT_H

#include "settings.h"
#include "ServoController.h"
#include <ESP32Servo.h>  // ESP32Servo library instead of Servo
/***********************************************************************************************
----------------------------    instrument.h   ----------------------------------------
************************************************************************************************

execute les messages noteOn et noteOff

************************************************************************************************/
class Instrument {
private:
  ServoController servoController;
  Servo airServo;            // Servo pour contrôle du débit d'air
  uint8_t activeNotesCount;  // Track number of active notes
  bool activeNotes[NUMBER_OF_NOTES];  // Track which notes are active
  uint8_t currentVolume;     // Current master volume (0-127)
  uint8_t currentAirAngle;   // Current air servo angle
  int getServo(uint8_t midiNote); //renvoit le numero du servo de 1 a 32 et 0 si la note ne peut pas etre jouée
  void openAir(uint8_t note, uint8_t velocity); // ouvre l'air en fonction de la note et de la velocité
  void closeAir(); // ferme les valves d'air
  void updateAirFlow(); // Met à jour le débit d'air selon les notes actives

public:
  Instrument();
  bool begin(); // Initialize instrument, returns true on success
  void noteOn(uint8_t midiNote, uint8_t velocity);
  void noteOff(uint8_t midiNote);
  void update();

  // Additional MIDI message handlers
  void allNotesOff(); // CC 123 - Stop all notes immediately
  void reset(); // CC 121 - Reset all controllers
  void volumeControl(uint8_t value); // CC 7 - Master volume
  void modulationWheel(uint8_t value); // CC 1, 91, 92, 94 - Modulation/Effects
  void pitchBend(int16_t value); // Pitch bend message
};

#endif // INSTRUMENT_H
