/***********************************************************************************************
 *  SERVO MELODICA - ESP32 WiFi MIDI VERSION
 ***********************************************************************************************
 *
 *  Système construit pour le contrôle d'un mélodica de 32 notes avec des servomoteurs SG90
 *  et deux cartes PCA9685, utilisant ESP32 avec WiFi MIDI (RTP-MIDI / AppleMIDI).
 *
 *  - WiFi MIDI : Communication MIDI via réseau WiFi (compatible macOS, Windows, iOS)
 *  - MidiHandler : Déchiffre les messages MIDI reçus via WiFi
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
 *  - AppleMIDI (by lathoub)
 *  - Adafruit PWM Servo Driver Library
 *  - ESP32Servo
 *
 ***********************************************************************************************/

#include <WiFi.h>
#include <AppleMIDI.h>
#include "Instrument.h"
#include "settings.h"

// WiFi credentials (configure in settings.h)
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// AppleMIDI instance
APPLEMIDI_CREATE_INSTANCE(WiFiUDP, MIDI, "Servo Melodica", DEFAULT_CONTROL_PORT);

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
  Serial.println("║     SERVO MELODICA - ESP32 WiFi MIDI (RTP-MIDI)         ║");
  Serial.println("╚══════════════════════════════════════════════════════════╝");

  // Connect to WiFi
  Serial.print("\nConnecting to WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  uint8_t attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n✗ WiFi connection failed!");
    Serial.println("Please check WIFI_SSID and WIFI_PASSWORD in settings.h");
    while (1) {
      delay(1000);
    }
  }

  Serial.println("\n✓ WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Initialize instrument
  Serial.println("\nInitializing Instrument...");
  instrument = new Instrument();

  if (!instrument->begin()) {
    Serial.println("ERROR: Instrument initialization failed!");
    while (1) {
      delay(1000);
    }
  }

  // Initialize AppleMIDI (RTP-MIDI)
  Serial.println("\nInitializing RTP-MIDI...");
  MIDI.begin(MIDI_CHANNEL_OMNI);

  // Setup AppleMIDI callbacks
  AppleMIDI.setHandleConnected([](const APPLEMIDI_NAMESPACE::ssrc_t & ssrc, const char* name) {
    Serial.print("✓ MIDI Connected to session: ");
    Serial.println(name);
  });

  AppleMIDI.setHandleDisconnected([](const APPLEMIDI_NAMESPACE::ssrc_t & ssrc) {
    Serial.println("✗ MIDI Disconnected");
    instrument->allNotesOff(); // Stop all notes on disconnect
  });

  // Setup MIDI callbacks
  MIDI.setHandleNoteOn(handleNoteOn);
  MIDI.setHandleNoteOff(handleNoteOff);
  MIDI.setHandleControlChange(handleControlChange);
  MIDI.setHandlePitchBend(handlePitchBend);

  Serial.println("✓ RTP-MIDI initialized");
  Serial.println("\n╔══════════════════════════════════════════════════════════╗");
  Serial.println("║  READY - Waiting for MIDI connection...                 ║");
  Serial.print("║  Connect to: ");
  Serial.print(WiFi.localIP());
  Serial.println("                        ║");
  Serial.println("║  Device name: 'Servo Melodica'                           ║");
  Serial.println("╚══════════════════════════════════════════════════════════╝\n");
}

void loop() {
  // Read and process MIDI messages
  MIDI.read();

  // Update instrument (for time-based operations)
  instrument->update();
}
