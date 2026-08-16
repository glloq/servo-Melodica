#ifndef TRANSPORTS_SERIALMIDITRANSPORT_H
#define TRANSPORTS_SERIALMIDITRANSPORT_H

#if defined(ARDUINO)

#include <Arduino.h>

#include "EventQueue.h"
#include "IMidiTransport.h"
#include "MidiLinkWatchdog.h"
#include "MidiParser.h"

namespace melodica {

// Raw MIDI bytes over a dedicated hardware UART, typically from a Raspberry Pi
// host running a MIDI-to-serial bridge. It owns its own UART (default Serial2 at
// 115200) so it never shares a port with the log/console on Serial — mixing MIDI
// bytes and log text on one UART would corrupt both.
//
// Link supervision uses the standard MIDI heartbeat rather than a made-up one:
// the bridge on the host sends Active Sensing (0xFE) every ~250 ms if it wants
// the instrument to stop on host loss. Without that heartbeat an open UART is
// considered connected, so a long held note is never cut short (see
// MidiLinkWatchdog). docs/WIRING.md documents the bridge side.
class SerialMidiTransport : public IMidiTransport {
public:
    explicit SerialMidiTransport(HardwareSerial& uart, uint32_t baud = 115200,
                                 int8_t rxPin = -1, int8_t txPin = -1,
                                 uint16_t activeSensingTimeoutMs = 300)
        : uart_(uart), baud_(baud), rxPin_(rxPin), txPin_(txPin),
          timeoutMs_(activeSensingTimeoutMs) {}

    bool begin() override {
        uart_.begin(baud_, SERIAL_8N1, rxPin_, txPin_);
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

    const char* name() const override { return "Serial-MIDI"; }

    bool supervised() const { return link_.supervised(); }

private:
    HardwareSerial& uart_;
    uint32_t baud_;
    int8_t rxPin_;
    int8_t txPin_;
    uint16_t timeoutMs_;
    MidiParser parser_;
    EventQueue<64> queue_;
    MidiLinkWatchdog link_;
};

}  // namespace melodica

#endif  // ARDUINO
#endif  // TRANSPORTS_SERIALMIDITRANSPORT_H
