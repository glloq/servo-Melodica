 #include "midiHandler.h"

MidiHandler::MidiHandler(Instrument &instrument) : _instrument(instrument) {
  if (DEBUG) {
    Serial.println("DEBUG : midiHandler--creation");
  } 
}

void MidiHandler::readMidi() {
  midiEventPacket_t midiEvent;
  do {
    midiEvent = MidiUSB.read();
    if (midiEvent.header != 0) {
      processMidiEvent(midiEvent);
    }
  } while (midiEvent.header != 0);
}

void MidiHandler::processMidiEvent(midiEventPacket_t midiEvent) {
  byte messageType = midiEvent.byte1 & 0xF0;
  byte channel = midiEvent.byte1 & 0x0F;
  byte note = midiEvent.byte2;
  byte velocity = midiEvent.byte3;

  switch (messageType) {
    case 0x90: // Note On
      if (velocity > 0) {
        _instrument.noteOn(note, velocity);
      } else {
        // Note Off
        _instrument.noteOff(note);
      }
      break;
    case 0x80: // Note Off
      _instrument.noteOff(note);
      break;
    case 0xE0: // Pitch Bend
      {
        int16_t pitchBendValue = ((midiEvent.byte3 << 7) | midiEvent.byte2) - 8192;
        _instrument.pitchBend(pitchBendValue);
      }
      break;
    case 0xA0: // Channel Pressure (Aftertouch)
      // Aftertouch could be used for expression control
      // Not implemented for melodica
      break;
    case 0xD0: // Polyphonic Key Pressure
      // Polyphonic aftertouch
      // Not implemented for melodica
      break;
    case 0xB0: // Control Change
      processControlChange(note, velocity);
      break;
    case 0xF0: // System Common or System Real-Time
      // Add logic for handling System Common and System Real-Time messages
      break;
    case 0xF7: // End of System Exclusive
      // Add logic for handling the end of System Exclusive message
      break;
    // Add more cases as needed for other message types
  }
}
/*------------------------------------------------------------------
--------------        process Control Change             ----------
------------------------------------------------------------------*/
void MidiHandler::processControlChange(byte controller, byte value) {
  // Handle Control Change messages
  switch (controller) {
    case 1:   // Modulation wheel
    case 91:  // Reverb depth
    case 92:  // Tremolo depth
    case 94:  // Detune depth
      _instrument.modulationWheel(value);
      break;
    case 0x07: // Volume (CC 7)
      _instrument.volumeControl(value);
      break;
    case 121: // Reset all controllers
      _instrument.reset();
      break;
    case 123: // All notes off
      _instrument.allNotesOff();
      break;
    case 120: // All sound off (similar to all notes off)
      _instrument.allNotesOff();
      break;
    // Add more cases as needed for other control changes
    default:
      if (DEBUG) {
        Serial.print("Unhandled CC: ");
        Serial.print(controller);
        Serial.print(" Value: ");
        Serial.println(value);
      }
      break;
  }
}
