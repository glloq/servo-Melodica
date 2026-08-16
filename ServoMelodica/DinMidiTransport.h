#ifndef TRANSPORTS_DINMIDITRANSPORT_H
#define TRANSPORTS_DINMIDITRANSPORT_H

#if defined(ARDUINO)

#include <Arduino.h>

#include "EventQueue.h"
#include "IMidiTransport.h"
#include "MidiLinkWatchdog.h"
#include "MidiParser.h"

namespace melodica {

// Classic 5-pin DIN MIDI over a hardware UART at 31250 baud.
//
// A DIN input carries no idle traffic, so silence is NOT a disconnection: the
// link counts as connected as soon as the UART is open and stays connected
// through a five-second held note. Supervision is only armed when the sender
// uses Active Sensing (0xFE) — see MidiLinkWatchdog for the full rationale.
class DinMidiTransport : public IMidiTransport {
public:
    explicit DinMidiTransport(HardwareSerial& uart, int8_t rxPin = -1, int8_t txPin = -1,
                              uint16_t activeSensingTimeoutMs = 300)
        : uart_(uart), rxPin_(rxPin), txPin_(txPin), timeoutMs_(activeSensingTimeoutMs) {}

    bool begin() override {
        uart_.begin(31250, SERIAL_8N1, rxPin_, txPin_);
        link_.setOpen(true);
        return true;
    }

    void update() override {
        MidiEvent e;
        const uint32_t nowMs = millis();
        while (uart_.available() > 0) {
            uint8_t b = static_cast<uint8_t>(uart_.read());
            link_.noteByte(b, nowMs);
            if (parser_.feed(b, micros(), e)) queue_.push(e);
        }
    }

    bool read(MidiEvent& event) override { return queue_.pop(event); }

    bool connected() const override { return link_.connected(millis(), timeoutMs_); }

    const char* name() const override { return "DIN-MIDI"; }

    // True once the sender opted into Active Sensing supervision (status UI).
    bool supervised() const { return link_.supervised(); }

private:
    HardwareSerial& uart_;
    int8_t rxPin_;
    int8_t txPin_;
    uint16_t timeoutMs_;
    MidiParser parser_;
    EventQueue<64> queue_;
    MidiLinkWatchdog link_;
};

}  // namespace melodica

#endif  // ARDUINO
#endif  // TRANSPORTS_DINMIDITRANSPORT_H
