#ifndef DRIVERS_SIMULATIONAIRCONTROLLER_H
#define DRIVERS_SIMULATIONAIRCONTROLLER_H

#include "AirControllerBase.h"
#include "IAirController.h"
#include "Log.h"

namespace melodica {

// Air module with no physical actuator: it runs the full AirRamp shaping and
// exposes the resulting level for inspection/logging. Used by the native tests
// and by the `native_tests` / bring-up profiles where no air hardware exists.
class SimulationAirController : public AirControllerBase {
public:
    explicit SimulationAirController(const AirConfig& cfg)
        : AirControllerBase(cfg), cfg_(cfg) {}

    // Refresh the ramp from the (possibly runtime-loaded) config before use.
    bool begin() override {
        ramp_.setConfig(cfg_);
        fault_ = AirFault::None;
        return true;
    }
    void update(uint32_t nowUs) override { level_ = ramp_.update(nowUs); }
    void emergencyStop() override { level_ = ramp_.forceClosed(); }

    uint8_t level() const { return level_; }

private:
    const AirConfig& cfg_;
    uint8_t level_ = 0;
};

}  // namespace melodica

#endif  // DRIVERS_SIMULATIONAIRCONTROLLER_H
