#ifndef CORE_AIRDEMAND_H
#define CORE_AIRDEMAND_H

#include <cstdint>

namespace melodica {

// Normalised request produced by the instrument core and consumed by any
// IAirController implementation. It is deliberately actuator-agnostic: the core
// says "how much sound is wanted", each air module decides how to realise it.
struct AirDemand {
    uint8_t expression = 0;       // 0..255 effective loudness (velocity*vol*expr)
    uint8_t maxVelocity = 0;      // loudest currently-active note velocity (0..127)
    uint8_t activeNoteCount = 0;  // number of sounding notes
    bool soundRequested = false;  // false => air must be fully closed
};

// Strategy used to turn the set of active notes + controllers into an
// AirDemand.expression value.
enum class AirPolicy : uint8_t {
    Fixed,                  // constant flow whenever any note sounds
    MaximumVelocity,        // follow the loudest active note
    AverageVelocity,        // follow the mean velocity of active notes
    SumClamped,             // sum velocities, clamp to full scale
    PolyphonyCompensated    // max velocity with a mild boost per extra note
};

}  // namespace melodica

#endif  // CORE_AIRDEMAND_H
