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
      //int pitchBendValue = (midiEvent.byte3 << 7) | midiEvent.byte2;
      //_instrument.pitchBend(pitchBendValue);
      break;
    case 0xA0: // Channel Pressure (Aftertouch)
      //int channelPressureValue = midiEvent.byte2;
      //_instrument.channelPressure(channelPressureValue);
      break;
    case 0xD0: // Polyphonic Key Pressure
      //int polyKeyPressureValue = midiEvent.byte3;
      //_instrument.polyKeyPressure(polyKeyPressureValue);
      break;
    case 0xB0: // Control Change
      processControlChange(note, velocity);
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
  // Add logic for handling Control Change messages
  // You can switch on the controller value to handle specific control changes
  switch (controller) {
     case 1:
    case 91:
    case 92:
    case 94: // Gestion du vibrato
      //_instrument.modulationWheel(value);
      break;
    case 0x07: // Volume
      //_instrument.volumeControl(value);
      break;
    case 121: // Réinitialisation de tous les contrôleurs
      //_instrument.reset();
      break;
    case 123: // Désactiver toutes les notes
      //_instrument.mute();
      break;
    // Add more cases as needed for other control changes
  }
}
