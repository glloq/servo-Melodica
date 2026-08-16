#ifndef CORE_AIRRAMP_H
#define CORE_AIRRAMP_H

#include <cstdint>

#include "AirDemand.h"
#include "AirRunTracker.h"
#include "Config.h"

namespace melodica {

// Pure, host-testable actuator-output shaper shared by every air module.
//
// Turns the core's AirDemand into a smoothly-ramped output level (0..255) while
// honouring: start threshold, min/max output, precharge kick, attack/release
// ramp times and global inversion. Concrete air controllers just take this
// level and write it to their actuator, so all the timing behaviour lives in one
// tested place.
class AirRamp {
public:
    explicit AirRamp(const AirConfig& cfg) : cfg_(cfg) {}

    void setConfig(const AirConfig& cfg) { cfg_ = cfg; }

    void setDemand(const AirDemand& d) { demand_ = d; }

    // Advance the ramp. Returns the actuator level 0..255 (already inverted if
    // configured). Non-blocking; `nowUs` is micros().
    uint8_t update(uint32_t nowUs) {
        uint32_t dtMs = lastUs_ == 0 ? 0 : (nowUs - lastUs_) / 1000u;
        lastUs_ = nowUs;

        uint16_t target = targetLevel();

        if (target > level_) {
            // Attack (or precharge kick on the first opening).
            if (!wasOpen_ && cfg_.precharge > level_) {
                level_ = cfg_.precharge;
            }
            level_ = rampToward(level_, target, cfg_.attackRampMs, dtMs);
            wasOpen_ = true;
        } else if (target < level_) {
            level_ = rampToward(level_, target, cfg_.releaseRampMs, dtMs);
            if (target == 0 && level_ == 0) wasOpen_ = false;
        }

        // Duty tracking uses the PRE-inversion level: "running" means air is
        // flowing, whichever way the actuator is wired.
        run_.update(nowUs, level_ > 0);

        return applyInversion(static_cast<uint8_t>(level_));
    }

    // Immediate hard close (safety). Returns the (possibly inverted) zero level.
    uint8_t forceClosed() {
        level_ = 0;
        wasOpen_ = false;
        demand_ = AirDemand{};
        run_.reset();
        return applyInversion(0);
    }

    uint8_t rawLevel() const { return static_cast<uint8_t>(level_); }

    // Duty-cycle introspection, used by the pump/blower overrun watchdog.
    bool isRunning() const { return run_.running(); }
    uint32_t runtimeMs() const { return run_.runtimeMs(); }

private:
    uint16_t targetLevel() const {
        if (!demand_.soundRequested || demand_.expression < cfg_.startThreshold) {
            return 0;
        }
        // Map expression (0..255) into [minOutput, maxOutput].
        uint32_t span = cfg_.maxOutput >= cfg_.minOutput
                            ? static_cast<uint32_t>(cfg_.maxOutput - cfg_.minOutput)
                            : 0;
        uint16_t out = static_cast<uint16_t>(cfg_.minOutput +
                                             (span * demand_.expression) / 255u);
        if (out > cfg_.maxOutput) out = cfg_.maxOutput;
        return out;
    }

    static uint16_t rampToward(uint16_t from, uint16_t to, uint16_t rampMs, uint32_t dtMs) {
        if (rampMs == 0 || dtMs == 0) return to;
        // Full-scale (255) traversal takes rampMs; scale the step to dt.
        uint32_t step = (255u * dtMs) / rampMs;
        if (step == 0) step = 1;
        if (from < to) {
            return (static_cast<uint32_t>(to - from) <= step) ? to
                                                              : static_cast<uint16_t>(from + step);
        }
        return (static_cast<uint32_t>(from - to) <= step) ? to
                                                          : static_cast<uint16_t>(from - step);
    }

    uint8_t applyInversion(uint8_t v) const {
        return cfg_.inverted ? static_cast<uint8_t>(255 - v) : v;
    }

    AirConfig cfg_;
    AirDemand demand_{};
    uint16_t level_ = 0;  // 0..255 pre-inversion
    uint32_t lastUs_ = 0;
    bool wasOpen_ = false;
    AirRunTracker run_;
};

}  // namespace melodica

#endif  // CORE_AIRRAMP_H
