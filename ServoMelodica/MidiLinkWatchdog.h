#ifndef CORE_MIDILINKWATCHDOG_H
#define CORE_MIDILINKWATCHDOG_H

#include <cstdint>

namespace melodica {

// Link-state logic for byte-stream MIDI transports (5-pin DIN, serial-from-Pi).
//
// WHY THIS EXISTS
// ---------------
// A byte-stream MIDI link carries NO idle traffic: after a Note On the sender is
// perfectly entitled to say nothing at all until the matching Note Off. Deriving
// "connected" from the age of the last received byte therefore breaks every note
// held longer than the timeout — the main loop sees connected() go false, calls
// onTransportLost() and releases the key mid-note.
//
// The rule implemented here is the one the MIDI spec actually gives us:
//
//   * The link is UP from the moment the UART is open. Silence proves nothing.
//   * A sender that emits Active Sensing (0xFE) is promising to keep sending at
//     least every 300 ms. Only THAT promise arms supervision: once a 0xFE has
//     been seen, going quiet for longer than the timeout does mean the cable is
//     unplugged or the sender is gone, and the link is reported DOWN.
//   * Any byte received afterwards brings the link back up (supervision stays
//     armed, since the sender previously opted in).
//
// Pure and host-testable: no Arduino types, no clock of its own.
class MidiLinkWatchdog {
public:
    static constexpr uint8_t kActiveSensing = 0xFE;

    // The UART is open (or not). Until this is true the link is always down.
    void setOpen(bool open) { open_ = open; }
    bool isOpen() const { return open_; }

    // Feed every received byte, with the current millis().
    void noteByte(uint8_t byte, uint32_t nowMs) {
        lastByteMs_ = nowMs;
        sawByte_ = true;
        if (byte == kActiveSensing) {
            armed_ = true;
            lastSenseMs_ = nowMs;
        }
    }

    // Current link state. `timeoutMs` is the Active-Sensing timeout (300 ms is
    // the value in the MIDI spec); 0 disables supervision entirely.
    bool connected(uint32_t nowMs, uint16_t timeoutMs) const {
        if (!open_) return false;
        if (!armed_ || timeoutMs == 0) return true;  // no promise made => always up
        return static_cast<uint32_t>(nowMs - lastByteMs_) <= timeoutMs;
    }

    // True once the sender opted into Active Sensing supervision.
    bool supervised() const { return armed_; }
    bool everReceived() const { return sawByte_; }
    uint32_t lastByteMs() const { return lastByteMs_; }
    uint32_t lastActiveSensingMs() const { return lastSenseMs_; }

    void reset() {
        armed_ = false;
        sawByte_ = false;
        lastByteMs_ = 0;
        lastSenseMs_ = 0;
    }

private:
    bool open_ = false;
    bool armed_ = false;
    bool sawByte_ = false;
    uint32_t lastByteMs_ = 0;
    uint32_t lastSenseMs_ = 0;
};

}  // namespace melodica

#endif  // CORE_MIDILINKWATCHDOG_H
