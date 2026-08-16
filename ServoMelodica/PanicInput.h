#ifndef CORE_PANICINPUT_H
#define CORE_PANICINPUT_H

#include <cstdint>

namespace melodica {

// Debounce + edge detection for a dedicated hardware emergency stop.
//
// The BOOT button is a configuration control: it only acts on RELEASE, and a
// press between 1.5 s and 3 s does nothing at all. That is fine for opening the
// setup hotspot and unacceptable for an emergency stop, which must fire the
// instant the operator hits it — while it is still held, not when they let go.
//
// This class turns a raw pin level into a debounced state and reports the
// transition INTO the active state. Recommended wiring is a normally-closed
// mushroom button between the pin and GND with the internal pull-up: pressing it
// (or cutting the wire) opens the loop and the pin reads HIGH, so a broken cable
// stops the instrument instead of silently disabling its emergency stop.
//
// Pure and host-testable: it is given the raw level and the time, and owns no
// hardware access of its own.
class PanicInput {
public:
    // Feed the debounced-in level on every loop. Returns true exactly once per
    // transition into the active state.
    bool update(uint32_t nowMs, bool rawActive, uint16_t debounceMs) {
        if (rawActive != candidate_) {
            candidate_ = rawActive;
            changedAtMs_ = nowMs;
            settled_ = false;
            return false;
        }
        if (settled_) return false;
        if (static_cast<uint32_t>(nowMs - changedAtMs_) < debounceMs) return false;

        settled_ = true;
        bool wasActive = active_;
        active_ = candidate_;
        if (!initialised_) {
            // The first settled reading only establishes the resting state: a
            // board that boots with the button already pressed must not silently
            // arm, it reports the fault on this very first evaluation.
            initialised_ = true;
            return active_;
        }
        return active_ && !wasActive;
    }

    bool active() const { return active_; }

private:
    bool candidate_ = false;
    bool active_ = false;
    bool settled_ = false;
    bool initialised_ = false;
    uint32_t changedAtMs_ = 0;
};

}  // namespace melodica

#endif  // CORE_PANICINPUT_H
