#ifndef TRANSPORTS_USBMIDITRANSPORT_H
#define TRANSPORTS_USBMIDITRANSPORT_H

// Native USB-MIDI is only meaningful on ESP32 variants that expose a USB
// peripheral (ESP32-S2 / ESP32-S3). The composition root defines
// MELODICA_USB_MIDI only for those boards, so this adapter is never compiled for
// the classic ESP32 (which has no device-mode USB).
#if defined(ARDUINO) && defined(MELODICA_USB_MIDI)

#include <Arduino.h>

#include "EventQueue.h"
#include "IMidiTransport.h"

namespace melodica {

// Adapter over an Arduino-MIDI interface bound to a TinyUSB MIDI device. Same
// static-trampoline pattern as the BLE adapter.
template <typename MidiType>
class UsbMidiTransport : public IMidiTransport {
public:
    explicit UsbMidiTransport(MidiType& midi) : midi_(midi) { self_ = this; }

    bool begin() override {
        midi_.begin(MIDI_CHANNEL_OMNI);
        midi_.setHandleNoteOn(&UsbMidiTransport::onNoteOn);
        midi_.setHandleNoteOff(&UsbMidiTransport::onNoteOff);
        midi_.setHandleControlChange(&UsbMidiTransport::onControlChange);
        midi_.setHandlePitchBend(&UsbMidiTransport::onPitchBend);
        midi_.setHandleProgramChange(&UsbMidiTransport::onProgramChange);
        return true;
    }

    void update() override { midi_.read(); }
    bool read(MidiEvent& event) override { return queue_.pop(event); }
    // USB-MIDI has no session concept; treat an enumerated device as connected.
    bool connected() const override { return true; }
    const char* name() const override { return "USB-MIDI"; }

private:
    static void push(const MidiEvent& e) { if (self_) self_->queue_.push(e); }
    static void onNoteOn(byte ch, byte n, byte v) {
        MidiEvent e; e.type = MidiEventType::NoteOn; e.channel = ch - 1;
        e.data1 = n; e.data2 = v; e.timestampUs = micros(); push(e);
    }
    static void onNoteOff(byte ch, byte n, byte v) {
        MidiEvent e; e.type = MidiEventType::NoteOff; e.channel = ch - 1;
        e.data1 = n; e.data2 = v; e.timestampUs = micros(); push(e);
    }
    static void onControlChange(byte ch, byte num, byte val) {
        MidiEvent e; e.type = MidiEventType::ControlChange; e.channel = ch - 1;
        e.data1 = num; e.data2 = val; e.timestampUs = micros(); push(e);
    }
    static void onPitchBend(byte ch, int bend) {
        MidiEvent e; e.type = MidiEventType::PitchBend; e.channel = ch - 1;
        e.signedValue = static_cast<int16_t>(bend); e.timestampUs = micros(); push(e);
    }
    static void onProgramChange(byte ch, byte program) {
        MidiEvent e; e.type = MidiEventType::ProgramChange; e.channel = ch - 1;
        e.data1 = program; e.timestampUs = micros(); push(e);
    }

    MidiType& midi_;
    EventQueue<64> queue_;
    static inline UsbMidiTransport* self_ = nullptr;
};

}  // namespace melodica

#endif  // ARDUINO && MELODICA_USB_MIDI
#endif  // TRANSPORTS_USBMIDITRANSPORT_H
