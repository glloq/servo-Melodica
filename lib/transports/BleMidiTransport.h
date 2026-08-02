#ifndef TRANSPORTS_BLEMIDITRANSPORT_H
#define TRANSPORTS_BLEMIDITRANSPORT_H

#if defined(ARDUINO)

#include <Arduino.h>

#include "EventQueue.h"
#include "IMidiTransport.h"

namespace melodica {

// Adapter over the Arduino-MIDI / ESP32-BLE-MIDI stack. Templated on the MIDI
// interface and BLE transport types so it works with either the Bluedroid or
// the NimBLE backend (selected by a build flag in the composition root) without
// naming the library's macro-generated types here.
//
// MIDI callbacks are C-style function pointers, so the decoded events are pushed
// into a lock-free queue via static trampolines that reference the singleton
// instance. The instrument core then drains the queue in the main loop.
template <typename MidiType, typename BleType>
class BleMidiTransport : public IMidiTransport {
public:
    BleMidiTransport(MidiType& midi, BleType& ble) : midi_(midi), ble_(ble) {
        self_ = this;
    }

    bool begin() override {
        midi_.begin(MIDI_CHANNEL_OMNI);
        ble_.setHandleConnected([]() { if (self_) self_->connected_ = true; });
        ble_.setHandleDisconnected([]() { if (self_) self_->connected_ = false; });
        midi_.setHandleNoteOn(&BleMidiTransport::onNoteOn);
        midi_.setHandleNoteOff(&BleMidiTransport::onNoteOff);
        midi_.setHandleControlChange(&BleMidiTransport::onControlChange);
        midi_.setHandlePitchBend(&BleMidiTransport::onPitchBend);
        midi_.setHandleProgramChange(&BleMidiTransport::onProgramChange);
        return true;
    }

    void update() override { midi_.read(); }
    bool read(MidiEvent& event) override { return queue_.pop(event); }
    bool connected() const override { return connected_; }
    const char* name() const override { return "BLE-MIDI"; }

private:
    static void push(const MidiEvent& e) { if (self_) self_->queue_.push(e); }

    static void onNoteOn(byte ch, byte note, byte vel) {
        MidiEvent e; e.type = MidiEventType::NoteOn; e.channel = ch - 1;
        e.data1 = note; e.data2 = vel; e.timestampUs = micros(); push(e);
    }
    static void onNoteOff(byte ch, byte note, byte vel) {
        MidiEvent e; e.type = MidiEventType::NoteOff; e.channel = ch - 1;
        e.data1 = note; e.data2 = vel; e.timestampUs = micros(); push(e);
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
    BleType& ble_;
    EventQueue<64> queue_;
    volatile bool connected_ = false;
    static inline BleMidiTransport* self_ = nullptr;
};

}  // namespace melodica

#endif  // ARDUINO
#endif  // TRANSPORTS_BLEMIDITRANSPORT_H
