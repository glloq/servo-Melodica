#ifndef CORE_IMIDITRANSPORT_H
#define CORE_IMIDITRANSPORT_H

#include "MidiEvent.h"

namespace melodica {

// Common interface for every MIDI transport. The instrument core only ever
// talks to this interface, so BLE / RTP-MIDI / DIN / USB / serial are fully
// interchangeable and none of their libraries leak into the core.
class IMidiTransport {
public:
    virtual ~IMidiTransport() = default;

    // One-time bring-up. Must be non-blocking: a transport that needs a network
    // link (WiFi/BLE) returns true once its state machine has started and
    // reports link status through connected() later.
    virtual bool begin() = 0;

    // Pump the transport. Called every loop() iteration; must never block.
    virtual void update() = 0;

    // Pop one decoded event. Returns false when no event is pending.
    virtual bool read(MidiEvent& event) = 0;

    // True when a peer/session is currently connected.
    virtual bool connected() const = 0;

    // Human-readable transport name for logs/UI.
    virtual const char* name() const = 0;
};

}  // namespace melodica

#endif  // CORE_IMIDITRANSPORT_H
