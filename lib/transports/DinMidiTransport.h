#ifndef TRANSPORTS_DINMIDITRANSPORT_H
#define TRANSPORTS_DINMIDITRANSPORT_H

#if defined(ARDUINO)

#include <Arduino.h>

#include "EventQueue.h"
#include "IMidiTransport.h"
#include "MidiParser.h"

namespace melodica {

// Classic 5-pin DIN MIDI over a hardware UART at 31250 baud. A DIN input is
// always "connected" in the network sense; connected() reports whether any
// bytes have been seen recently so the safety watchdog can react to an
// unplugged cable.
class DinMidiTransport : public IMidiTransport {
public:
    explicit DinMidiTransport(HardwareSerial& uart, int8_t rxPin = -1, int8_t txPin = -1)
        : uart_(uart), rxPin_(rxPin), txPin_(txPin) {}

    bool begin() override {
        uart_.begin(31250, SERIAL_8N1, rxPin_, txPin_);
        return true;
    }

    void update() override {
        MidiEvent e;
        while (uart_.available() > 0) {
            uint8_t b = static_cast<uint8_t>(uart_.read());
            lastByteMs_ = millis();
            if (parser_.feed(b, micros(), e)) queue_.push(e);
        }
    }

    bool read(MidiEvent& event) override { return queue_.pop(event); }

    bool connected() const override {
        return lastByteMs_ != 0 && (millis() - lastByteMs_) < 2000;
    }

    const char* name() const override { return "DIN-MIDI"; }

private:
    HardwareSerial& uart_;
    int8_t rxPin_;
    int8_t txPin_;
    MidiParser parser_;
    EventQueue<64> queue_;
    uint32_t lastByteMs_ = 0;
};

}  // namespace melodica

#endif  // ARDUINO
#endif  // TRANSPORTS_DINMIDITRANSPORT_H
