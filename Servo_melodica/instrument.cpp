#include "Instrument.h"

Instrument::Instrument() : servoController(), activeNotesCount(0), currentVolume(127) {
  if (DEBUG) {
    Serial.println("DEBUG: Instrument--creation");
  }
  // Initialize air valve pins
  pinMode(PIN_VALVE1, OUTPUT);
  pinMode(PIN_VALVE2, OUTPUT);
  closeAir();

  // Initialize active notes array
  for (uint8_t i = 0; i < NUMBER_OF_NOTES; i++) {
    activeNotes[i] = false;
  }
}

bool Instrument::begin() {
  Serial.println("Initializing Instrument...");

  // Initialize servo controller
  if (!servoController.begin()) {
    Serial.println("ERROR: Failed to initialize ServoController!");
    return false;
  }

  Serial.println("Instrument initialized successfully");
  return true;
}

int Instrument::getServo(uint8_t midiNote) {
  // Returns the servo number (0 to NUMBER_OF_NOTES-1) for the given MIDI note
  // Returns -1 if the note is not playable
  if ((midiNote >= FIRST_MIDI_NOTE) && (midiNote < (FIRST_MIDI_NOTE + NUMBER_OF_NOTES))) {
    int servoAJouer = midiNote - FIRST_MIDI_NOTE;
    return servoAJouer;
  }
  if (DEBUG) {
    Serial.print("DEBUG: MIDI note ");
    Serial.print(midiNote);
    Serial.println(" not playable");
  }
  return -1;
}

void Instrument::noteOn(uint8_t midiNote, uint8_t velocity) {
  int servo = getServo(midiNote);
  if (servo != -1) {
    // Apply volume scaling to velocity
    uint8_t scaledVelocity = (velocity * currentVolume) / 127;

    servoController.noteOn(servo, scaledVelocity);

    // Track active notes
    if (!activeNotes[servo]) {
      activeNotes[servo] = true;
      activeNotesCount++;
    }

    openAir(midiNote, scaledVelocity);
  }
}

void Instrument::noteOff(uint8_t midiNote) {
  int servo = getServo(midiNote);
  if (servo != -1) {
    // Remet le servo à sa position initiale
    servoController.noteOff(servo);

    // Track active notes
    if (activeNotes[servo]) {
      activeNotes[servo] = false;
      if (activeNotesCount > 0) {
        activeNotesCount--;
      }
    }

    // Close air if no more notes are playing
    if (activeNotesCount == 0) {
      closeAir();
    }
  }
}

void Instrument::openAir(uint8_t note, uint8_t velocity) {
  // Open air valves when a note is played
  // Could be made more sophisticated by considering velocity or note range

  // For melodica, we typically need air flow for all notes
  // PIN_VALVE1 and PIN_VALVE2 can be used for different pressure levels
  // or different sections of the keyboard

  if (velocity < 64) {
    // Lower velocity - use only valve 1 for softer playing
    digitalWrite(PIN_VALVE1, HIGH);
    digitalWrite(PIN_VALVE2, LOW);
  } else {
    // Higher velocity - use both valves for stronger air pressure
    digitalWrite(PIN_VALVE1, HIGH);
    digitalWrite(PIN_VALVE2, HIGH);
  }

  if (DEBUG) {
    Serial.print("Air opened - Note: ");
    Serial.print(note);
    Serial.print(" Velocity: ");
    Serial.print(velocity);
    Serial.print(" Active notes: ");
    Serial.println(activeNotesCount);
  }
}

void Instrument::closeAir() {
  // Close both air valves
  digitalWrite(PIN_VALVE1, LOW);
  digitalWrite(PIN_VALVE2, LOW);

  if (DEBUG) {
    Serial.println("Air closed - No active notes");
  }
}

void Instrument::update() {
  // This method can be called regularly from the main loop
  // for any time-based operations like:
  // - Envelope control
  // - Pressure management
  // - LED indicators
  // Currently empty but ready for future enhancements
}

// ========== ADDITIONAL MIDI MESSAGE HANDLERS ==========

void Instrument::allNotesOff() {
  // CC 123 - Stop all notes immediately (panic button)
  Serial.println("MIDI: All Notes Off");

  for (uint8_t i = 0; i < NUMBER_OF_NOTES; i++) {
    if (activeNotes[i]) {
      servoController.noteOff(i);
      activeNotes[i] = false;
    }
  }

  activeNotesCount = 0;
  closeAir();
}

void Instrument::reset() {
  // CC 121 - Reset all controllers to default state
  Serial.println("MIDI: Reset All Controllers");

  // Stop all notes
  allNotesOff();

  // Reset volume to maximum
  currentVolume = 127;

  // Reset servo controller to default calibration if needed
  // servoController.resetToDefaultCalibration();
}

void Instrument::volumeControl(uint8_t value) {
  // CC 7 - Master volume control (0-127)
  currentVolume = value;

  if (DEBUG) {
    Serial.print("MIDI: Volume set to ");
    Serial.println(value);
  }

  // Volume affects velocity scaling in noteOn()
  // No need to adjust currently playing notes for melodica
}

void Instrument::modulationWheel(uint8_t value) {
  // CC 1, 91, 92, 94 - Modulation/Effects
  // For melodica, could control vibrato or pressure variation

  if (DEBUG) {
    Serial.print("MIDI: Modulation value ");
    Serial.println(value);
  }

  // Could be used to add vibrato by slightly varying servo angles
  // or adjusting air pressure periodically
  // Implementation depends on hardware capabilities
  // Currently logged for future enhancement
}

void Instrument::pitchBend(int16_t value) {
  // Pitch bend message (-8192 to +8191)
  // For melodica, this is difficult to implement mechanically
  // Could slightly adjust servo pressure for subtle pitch variation

  if (DEBUG) {
    Serial.print("MIDI: Pitch bend value ");
    Serial.println(value);
  }

  // Pitch bend on a melodica could:
  // 1. Slightly adjust key pressure (limited effect)
  // 2. Momentarily open/close adjacent keys (complex)
  // 3. Control air pressure (if system supports it)
  // Currently logged for future enhancement
}