#ifndef CORE_DEFAULTS_H
#define CORE_DEFAULTS_H

#include "Config.h"
#include "PcaMapping.h"

namespace melodica {

// Compiled-in default configuration. This is the single source of factory
// defaults, used both as the fallback when NVS is empty/invalid and as the base
// for the serial calibration console.
inline BoardConfig makeDefaultConfig() {
    BoardConfig cfg;  // struct member initialisers already provide safe values
#ifdef DEFAULT_AIR_TYPE
    // A profile may set the initial air-system type (still runtime-changeable).
    cfg.air.type = static_cast<AirSystemType>(DEFAULT_AIR_TYPE);
#endif
    for (uint16_t i = 0; i < MAX_NOTES; ++i) {
        cfg.keys[i].restPulseUs = 1500;
        cfg.keys[i].pressedPulseUs = 1900;
        cfg.keys[i].minPulseUs = 500;
        cfg.keys[i].maxPulseUs = 2500;
        cfg.keys[i].inverted = false;
    }
    return cfg;
}

// Compile-time guarantee that the calibration table has exactly one entry per
// key slot, and that the hardware can address every slot.
static_assert(sizeof(BoardConfig::keys) / sizeof(KeyCalibration) == MAX_NOTES,
              "Key calibration table size must equal MAX_NOTES");
static_assert(MAX_NOTES == MAX_PCA_BOARDS * CHANNELS_PER_PCA9685,
              "MAX_NOTES must match the total PCA9685 channel count");
static_assert(NUMBER_OF_NOTES <= MAX_NOTES, "default note count exceeds cap");

}  // namespace melodica

#endif  // CORE_DEFAULTS_H
