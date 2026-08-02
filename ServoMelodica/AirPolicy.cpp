#include "AirPolicy.h"

namespace melodica {

namespace {
// Scale a 0..127 base value by CC7 and CC11 (each 0..127) into 0..255.
uint8_t applyControllers(uint16_t base127, uint8_t volumeCc7, uint8_t expressionCc11) {
    // base127 is 0..127. Two 0..127 multipliers => normalise back to 0..255.
    uint32_t scaled = static_cast<uint32_t>(base127) * volumeCc7 * expressionCc11;
    // Max = 127*127*127 = 2048383. Map that to 255.
    uint32_t out = (scaled * 255u) / (127u * 127u * 127u);
    if (out > 255u) out = 255u;
    return static_cast<uint8_t>(out);
}
}  // namespace

uint8_t computeAirExpression(AirPolicy policy,
                             const uint8_t* velocities,
                             uint8_t count,
                             uint8_t volumeCc7,
                             uint8_t expressionCc11) {
    if (count == 0 || volumeCc7 == 0 || expressionCc11 == 0) {
        return 0;  // silence => air fully closed
    }

    uint16_t base = 0;  // 0..127 pre-controller demand
    switch (policy) {
        case AirPolicy::Fixed:
            base = 100;  // constant nominal flow whenever any note sounds
            break;

        case AirPolicy::MaximumVelocity: {
            uint8_t maxv = 0;
            for (uint8_t i = 0; i < count; ++i) {
                if (velocities[i] > maxv) maxv = velocities[i];
            }
            base = maxv;
            break;
        }

        case AirPolicy::AverageVelocity: {
            uint32_t sum = 0;
            for (uint8_t i = 0; i < count; ++i) sum += velocities[i];
            base = static_cast<uint16_t>(sum / count);
            break;
        }

        case AirPolicy::SumClamped: {
            uint32_t sum = 0;
            for (uint8_t i = 0; i < count; ++i) sum += velocities[i];
            base = sum > 127u ? 127u : static_cast<uint16_t>(sum);
            break;
        }

        case AirPolicy::PolyphonyCompensated: {
            // Follow the loudest note, then add a mild boost for each additional
            // note so a full chord does not sound starved relative to one note.
            uint8_t maxv = 0;
            for (uint8_t i = 0; i < count; ++i) {
                if (velocities[i] > maxv) maxv = velocities[i];
            }
            uint16_t boost = static_cast<uint16_t>((count - 1) * 6);  // ~+6/note
            base = maxv + boost;
            if (base > 127u) base = 127u;
            break;
        }
    }

    return applyControllers(base, volumeCc7, expressionCc11);
}

}  // namespace melodica
