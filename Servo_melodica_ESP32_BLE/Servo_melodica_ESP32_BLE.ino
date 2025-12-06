/***********************************************************************************************
 *  SERVO MELODICA - ESP32 BLUETOOTH MIDI VERSION
 ***********************************************************************************************
 *
 *  Système construit pour le contrôle d'un mélodica de 32 notes avec des servomoteurs SG90
 *  et deux cartes PCA9685, utilisant ESP32 avec Bluetooth MIDI.
 *
 *  - BLE MIDI : Communication MIDI sans fil via Bluetooth Low Energy
 *  - MidiHandler : Déchiffre les messages MIDI reçus via BLE
 *  - Instrument : Vérifie et joue les notes
 *  - ServoController : Contrôle des servos via PCA9685
 *  - AirManager : Gère l'ouverture de la valve d'air selon la vélocité
 *
 *  MATÉRIEL REQUIS :
 *  - ESP32 (DevKit, WROOM, etc.)
 *  - 2× PCA9685 (I2C)
 *  - 32× Servos SG90 (touches)
 *  - 1× Servo SG90 (air)
 *
 *  BIBLIOTHÈQUES REQUISES :
 *  - ESP32-BLE-MIDI (by lathoub)
 *  - Adafruit PWM Servo Driver Library
 *  - ESP32Servo
 *
 ***********************************************************************************************/

#include <BLEMIDI_Transport.h>
#include <hardware/BLEMIDI_ESP32.h>
#include "Instrument.h"
#include "settings.h"

// BLE MIDI instance
BLEMIDI_CREATE_INSTANCE("Servo Melodica", MIDI);

Instrument* instrument = nullptr;

// MIDI callback handlers
void handleNoteOn(byte channel, byte note, byte velocity) {
  if (velocity == 0) {
    // Velocity 0 = Note Off
    instrument->noteOff(note);
  } else {
    instrument->noteOn(note, velocity);
  }
}

void handleNoteOff(byte channel, byte note, byte velocity) {
  instrument->noteOff(note);
}

void handleControlChange(byte channel, byte controller, byte value) {
  switch (controller) {
    case 7:   // Volume (CC 7)
      instrument->volumeControl(value);
      break;
    case 1:   // Modulation (CC 1)
    case 91:  // Reverb (CC 91)
    case 92:  // Tremolo (CC 92)
    case 94:  // Detune (CC 94)
      instrument->modulationWheel(value);
      break;
    case 121: // Reset All Controllers (CC 121)
      instrument->reset();
      break;
    case 123: // All Notes Off (CC 123)
      instrument->allNotesOff();
      break;
  }
}

void handlePitchBend(byte channel, int bend) {
  instrument->pitchBend(bend);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n╔══════════════════════════════════════════════════════════╗");
  Serial.println("║     SERVO MELODICA - ESP32 BLUETOOTH MIDI                ║");
  Serial.println("╚══════════════════════════════════════════════════════════╝");

  // Initialize instrument
  Serial.println("\nInitializing Instrument...");
  instrument = new Instrument();

  if (!instrument->begin()) {
    Serial.println("ERROR: Instrument initialization failed!");
    while (1) {
      delay(1000);
    }
  }

  // Initialize BLE MIDI
  Serial.println("\nInitializing BLE MIDI...");
  MIDI.begin(MIDI_CHANNEL_OMNI);

  // Setup MIDI callbacks
  BLEMIDI.setHandleConnected([]() {
    Serial.println("✓ BLE MIDI Connected!");
  });

  BLEMIDI.setHandleDisconnected([]() {
    Serial.println("✗ BLE MIDI Disconnected");
    instrument->allNotesOff(); // Stop all notes on disconnect
  });

  MIDI.setHandleNoteOn(handleNoteOn);
  MIDI.setHandleNoteOff(handleNoteOff);
  MIDI.setHandleControlChange(handleControlChange);
  MIDI.setHandlePitchBend(handlePitchBend);

  Serial.println("✓ BLE MIDI initialized");
  Serial.println("\n╔══════════════════════════════════════════════════════════╗");
  Serial.println("║  READY - Waiting for BLE MIDI connection...             ║");
  Serial.println("║  Device name: 'Servo Melodica'                           ║");
  Serial.println("╚══════════════════════════════════════════════════════════╝\n");
}

void loop() {
  // Read and process MIDI messages
  MIDI.read();

  // Update instrument (for time-based operations)
  instrument->update();
}
