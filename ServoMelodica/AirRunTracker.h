#ifndef CORE_AIRRUNTRACKER_H
#define CORE_AIRRUNTRACKER_H

#include <cstdint>

namespace melodica {

// Measures how long an air actuator has been running WITHOUT interruption.
//
// This is what turns SafetyConfig::maxPumpRunMs from a documented intention into
// an enforced limit: a pump, blower or reservoir compressor that never stops —
// because a sensor reads zero, a valve is stuck or the regulator set-point can
// never be reached — is a thermal and mechanical hazard, and nothing else in the
// firmware notices it (the key watchdogs only see keys).
//
// Pure and host-testable; wrap-safe across the ~71-minute micros() rollover
// because it only ever uses unsigned differences.
class AirRunTracker {
public:
    // Feed the actuator state on every update(). `running` is "this actuator is
    // moving air right now" (level > 0, pump energised, ...).
    void update(uint32_t nowUs, bool running) {
        if (running) {
            if (!running_) {
                running_ = true;
                startedUs_ = nowUs;
            }
            elapsedMs_ = (nowUs - startedUs_) / 1000u;
        } else {
            running_ = false;
            elapsedMs_ = 0;
        }
    }

    // Force the "off" state (emergency stop, reconfiguration).
    void reset() {
        running_ = false;
        elapsedMs_ = 0;
        startedUs_ = 0;
    }

    bool running() const { return running_; }

    // Length of the CURRENT uninterrupted run, in milliseconds (0 when off).
    uint32_t runtimeMs() const { return elapsedMs_; }

private:
    bool running_ = false;
    uint32_t startedUs_ = 0;
    uint32_t elapsedMs_ = 0;
};

}  // namespace melodica

#endif  // CORE_AIRRUNTRACKER_H
