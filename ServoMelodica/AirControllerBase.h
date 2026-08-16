#ifndef CORE_AIRCONTROLLERBASE_H
#define CORE_AIRCONTROLLERBASE_H

#include "AirRamp.h"
#include "IAirController.h"

namespace melodica {

// Shared plumbing for every ramp-driven air module: it owns the AirRamp (so all
// the shaping/timing stays in one tested place) and derives the supervision
// answers — isRunning(), runtimeMs(), healthy(), fault() — from it. A concrete
// module only implements bring-up and "write this 0..255 level to my actuator".
class AirControllerBase : public IAirController {
public:
    explicit AirControllerBase(const AirConfig& cfg) : ramp_(cfg) {}

    void setDemand(const AirDemand& d) override { ramp_.setDemand(d); }
    void stop() override { ramp_.setDemand(AirDemand{}); }

    bool isRunning() const override { return ramp_.isRunning(); }
    uint32_t runtimeMs() const override { return ramp_.runtimeMs(); }
    bool healthy() const override { return fault_ == AirFault::None; }
    AirFault fault() const override { return fault_; }

protected:
    // Record a bring-up failure; returns false so begin() can `return fail(...)`.
    bool fail(AirFault f) {
        fault_ = f;
        return false;
    }

    AirRamp ramp_;
    AirFault fault_ = AirFault::NotInitialised;  // cleared by a successful begin()
};

}  // namespace melodica

#endif  // CORE_AIRCONTROLLERBASE_H
