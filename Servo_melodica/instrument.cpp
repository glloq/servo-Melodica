#include "Instrument.h"

Instrument::Instrument() : servoController(), activeNotesCount(0), currentVolume(127), currentAirAngle(AIR_CLOSED_ANGLE) {
  if (DEBUG) {
    Serial.println("DEBUG: Instrument--creation");
  }

  // Initialize air servo
  airServo.attach(AIR_SERVO_PIN);
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
    // Apply volume scaling to velocity (for air servo only)
    uint8_t scaledVelocity = (velocity * currentVolume) / 127;

    // Appuie sur la touche (position fixe, pas de vélocité)
    servoController.noteOn(servo);

    // Track active notes
    if (!activeNotes[servo]) {
      activeNotes[servo] = true;
      activeNotesCount++;
    }

    // Vélocité gérée uniquement par le servo d'air
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
  // Ouvre la valve d'air en fonction de la vélocité
  // Plus la vélocité est forte, plus l'angle d'ouverture est grand

  // Map velocity (0-127) to air servo angle (AIR_MIN_ANGLE to AIR_MAX_ANGLE)
  uint8_t targetAngle = map(velocity, 1, 127, AIR_MIN_ANGLE, AIR_MAX_ANGLE);

  // Si l'angle demandé est supérieur à l'angle actuel, mettre à jour
  if (targetAngle > currentAirAngle) {
    currentAirAngle = targetAngle;
    airServo.write(currentAirAngle);
  }

  if (DEBUG) {
    Serial.print("Air opened - Note: ");
    Serial.print(note);
    Serial.print(" Velocity: ");
    Serial.print(velocity);
    Serial.print(" Angle: ");
    Serial.print(currentAirAngle);
    Serial.print(" Active notes: ");
    Serial.println(activeNotesCount);
  }
}

void Instrument::closeAir() {
  // Ferme la valve d'air
  currentAirAngle = AIR_CLOSED_ANGLE;
  airServo.write(currentAirAngle);

  if (DEBUG) {
    Serial.println("Air closed - No active notes");
  }
}

void Instrument::updateAirFlow() {
  // Met à jour le débit d'air en fonction des notes actives
  // Trouve la vélocité maximale parmi les notes actives
  // et ajuste l'angle du servo en conséquence

  if (activeNotesCount == 0) {
    closeAir();
  } else {
    // L'angle est déjà défini par openAir() lors du noteOn
    // Cette fonction peut être utilisée pour ajustements dynamiques
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