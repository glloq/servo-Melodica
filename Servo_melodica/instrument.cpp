#include "Instrument.h"

Instrument::Instrument() : servoController() {
  if (DEBUG) {
    Serial.println("DEBUG : Instrument--creation");
  } 
}

int Instrument::getServo(uint8_t midiNote) {
//renvoit le numero du servo en fct de la note recu si dans les notes jouables
if((midiNote<FIRST_MIDI_NOTE)&&(midiNote<(FIRST_MIDI_NOTE+NUMBER_OF_NOTES))){
  int servoAJouer = midiNote-FIRST_MIDI_NOTE;
  return servoAJouer;
}
  if (DEBUG) {
    Serial.println("DEBUG : instrument : midi note not playable");
  } 
	return -1;
}

void Instrument::noteOn(uint8_t midiNote, uint8_t velocity) {
	int servo=getServo(midiNote);
	if (servo!=-1){
		servoController.noteOn(servo);
	}
}

void Instrument::noteOff(uint8_t midiNote) {
  int servo=getServo(midiNote);
	if (servo!=-1){
		// Remet le servo à sa position initiale
		servoController.noteOff(servo);	    
  }
}

void Instrument::openAir(uint8_t note, uint8_t velocity){
  
}

void Instrument::update(){


}