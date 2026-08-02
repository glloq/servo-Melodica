#ifndef CORE_AIRPOLICY_H
#define CORE_AIRPOLICY_H

#include <cstdint>

#include "AirDemand.h"

namespace melodica {

// Pure, host-testable computation of the effective air expression (0..255).
//
// Inputs:
//   policy        : selected strategy
//   velocities    : velocities (1..127) of the currently active notes
//   count         : number of active notes
//   volumeCc7     : CC7 master volume (0..127)
//   expressionCc11: CC11 expression (0..127)
//
// The result folds in CC7 and CC11 multiplicatively. If either controller is 0,
// or no notes are active, the effective demand is 0 which fully closes the air.
// Because the set of active velocities is recomputed on every note change, a
// loud note ending while a soft note is still held makes the demand fall
// automatically.
uint8_t computeAirExpression(AirPolicy policy,
                             const uint8_t* velocities,
                             uint8_t count,
                             uint8_t volumeCc7,
                             uint8_t expressionCc11);

}  // namespace melodica

#endif  // CORE_AIRPOLICY_H
