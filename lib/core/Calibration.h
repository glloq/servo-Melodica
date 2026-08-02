#ifndef CORE_CALIBRATION_H
#define CORE_CALIBRATION_H

#include <cstdint>

#include "Config.h"

namespace melodica {

// ---------------------------------------------------------------------------
// Legacy calibration migration
// ---------------------------------------------------------------------------
// The original firmware stored calibration as an initial angle (degrees) plus a
// rotation direction (+1/-1) per servo, and pressed the key by moving
// ANGLE_NOTE_ON degrees in that direction. The unified firmware stores absolute
// pulse widths per key. This converts an old (angle,direction) pair into the new
// KeyCalibration so users upgrading from a previous EEPROM dump keep their work.
struct LegacyServoCalibration {
    uint16_t restAngleDeg;  // old initialAngles[i]
    int8_t direction;       // old sensRot[i] (+1 / -1)
};

// Map a servo angle (0..180) to a pulse width using the classic 500..2500us span.
inline uint16_t legacyAngleToPulseUs(uint16_t angleDeg) {
    if (angleDeg > 180) angleDeg = 180;
    // 500us at 0deg, 2500us at 180deg.
    return static_cast<uint16_t>(500u + (static_cast<uint32_t>(angleDeg) * 2000u) / 180u);
}

// Convert one legacy entry. `pressDeltaDeg` is the old ANGLE_NOTE_ON travel.
inline KeyCalibration migrateLegacyKey(const LegacyServoCalibration& old,
                                       uint16_t pressDeltaDeg) {
    int restAngle = old.restAngleDeg;
    int pressedAngle = restAngle - pressDeltaDeg * old.direction;
    if (pressedAngle < 0) pressedAngle = 0;
    if (pressedAngle > 180) pressedAngle = 180;

    KeyCalibration k;
    k.restPulseUs = legacyAngleToPulseUs(old.restAngleDeg);
    k.pressedPulseUs = legacyAngleToPulseUs(static_cast<uint16_t>(pressedAngle));
    k.minPulseUs = 500;
    k.maxPulseUs = 2500;
    k.inverted = false;  // direction is already baked into pressedPulseUs
    return k;
}

// ---------------------------------------------------------------------------
// CRC32 (IEEE 802.3) for config integrity checks — small table-free variant.
// ---------------------------------------------------------------------------
inline uint32_t crc32Update(uint32_t crc, const uint8_t* data, uint32_t len) {
    crc = ~crc;
    for (uint32_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            crc = (crc >> 1) ^ (0xEDB88320u & (~(crc & 1u) + 1u));
        }
    }
    return ~crc;
}

// CRC over a BoardConfig excluding its trailing crc32 field.
inline uint32_t computeConfigCrc(const BoardConfig& cfg) {
    const uint8_t* base = reinterpret_cast<const uint8_t*>(&cfg);
    uint32_t len = static_cast<uint32_t>(reinterpret_cast<const uint8_t*>(&cfg.crc32) - base);
    return crc32Update(0, base, len);
}

// Validate a loaded config: schema version supported and CRC matches.
inline bool validateConfig(const BoardConfig& cfg) {
    if (cfg.schemaVersion == 0 || cfg.schemaVersion > CONFIG_SCHEMA_VERSION) return false;
    return computeConfigCrc(cfg) == cfg.crc32;
}

}  // namespace melodica

#endif  // CORE_CALIBRATION_H
