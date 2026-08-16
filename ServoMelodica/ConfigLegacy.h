#ifndef CORE_CONFIGLEGACY_H
#define CORE_CONFIGLEGACY_H

#include <cstdint>

#include "Calibration.h"
#include "Config.h"

namespace melodica {

// Frozen snapshots of older persisted layouts, plus their forward migrations.
//
// The previous ConfigStore claimed to migrate but could not: it required the
// stored blob to be exactly sizeof(BoardConfig) before doing anything, so the
// very event migration exists for — a schema change that resizes the struct —
// made it fall straight through to the compiled defaults. Every stored
// calibration was silently lost on upgrade.
//
// The rule from now on:
//   * v3 and later are written with a ConfigRecordHeader (magic, schema, size,
//     CRC), so a record can be identified and length-checked BEFORE being
//     interpreted.
//   * every older layout keeps a frozen copy here and a field-by-field
//     migration into the current BoardConfig.
//
// Nothing in this namespace may ever be edited to follow a change in Config.h:
// these structs describe bytes already sitting in somebody's NVS.
namespace legacy {

// ---- Schema v2 (geometry + composite air) ---------------------------------
// Only KeyDriverConfig and SafetyConfig changed in v3; the other members are
// byte-identical, and the static_assert below fails if that ever stops being
// true, forcing whoever changes them to freeze a copy here.
struct KeyDriverConfigV2 {
    uint8_t boardCount;
    uint8_t addresses[MAX_PCA_BOARDS];
    int8_t oePin;
    uint16_t pwmFrequencyHz;
    uint16_t slewUsPerMs;
};

struct SafetyConfigV2 {
    uint32_t commTimeoutMs;
    uint32_t maxKeyHoldMs;
    uint32_t maxPumpRunMs;
    bool disableOutputsOnIdle;
    uint32_t idleDisableDelayMs;
};

struct BoardConfigV2 {
    uint16_t schemaVersion;
    InstrumentConfig instrument;
    I2cConfig i2c;
    KeyDriverConfigV2 keyDriver;
    AirConfig air;
    MidiFilterConfig midi;
    NetworkConfig network;
    SafetyConfigV2 safety;
    KeyCalibration keys[MAX_NOTES];
    uint32_t crc32;
};

// If one of the reused sub-structs is resized, this stops the build instead of
// silently mis-reading everybody's stored configuration.
static_assert(sizeof(BoardConfigV2) == 888,
              "v2 record layout changed: freeze the old sub-structs in ConfigLegacy.h");

inline uint32_t crcOfV2(const BoardConfigV2& cfg) {
    const uint8_t* base = reinterpret_cast<const uint8_t*>(&cfg);
    uint32_t len = static_cast<uint32_t>(reinterpret_cast<const uint8_t*>(&cfg.crc32) - base);
    return crc32Update(0, base, len);
}

inline bool validV2(const BoardConfigV2& cfg) {
    return cfg.schemaVersion == 2 && crcOfV2(cfg) == cfg.crc32;
}

// v2 -> current. Everything the operator configured is preserved; the fields
// introduced in v3 take their compiled defaults (they are new features, not
// changed meanings).
inline BoardConfig migrateV2(const BoardConfigV2& old) {
    BoardConfig out;  // member initialisers provide the v3 defaults
    out.schemaVersion = CONFIG_SCHEMA_VERSION;
    out.instrument = old.instrument;
    out.i2c = old.i2c;
    out.air = old.air;
    out.midi = old.midi;
    out.network = old.network;

    out.keyDriver.boardCount = old.keyDriver.boardCount;
    for (uint8_t i = 0; i < MAX_PCA_BOARDS; ++i) {
        out.keyDriver.addresses[i] = old.keyDriver.addresses[i];
    }
    out.keyDriver.oePin = old.keyDriver.oePin;
    out.keyDriver.pwmFrequencyHz = old.keyDriver.pwmFrequencyHz;
    out.keyDriver.slewUsPerMs = old.keyDriver.slewUsPerMs;
    // keyDriver.healthCheckIntervalMs: new in v3, keeps its default.

    out.safety.commTimeoutMs = old.safety.commTimeoutMs;
    out.safety.maxKeyHoldMs = old.safety.maxKeyHoldMs;
    out.safety.maxPumpRunMs = old.safety.maxPumpRunMs;
    out.safety.disableOutputsOnIdle = old.safety.disableOutputsOnIdle;
    out.safety.idleDisableDelayMs = old.safety.idleDisableDelayMs;
    // safety.activeSensingTimeoutMs / panicPin / panicActiveHigh /
    // panicDebounceMs: new in v3, keep their defaults (no panic pin wired).

    for (uint16_t i = 0; i < MAX_NOTES; ++i) out.keys[i] = old.keys[i];

    out.crc32 = computeConfigCrc(out);
    return out;
}

}  // namespace legacy

// ---------------------------------------------------------------------------
// Current on-NVS record
// ---------------------------------------------------------------------------
// Written from schema v3 onwards. Storing the schema AND the payload size AND a
// CRC in a fixed-size header is what makes a future migration possible: a record
// can be recognised and its version dispatched on before anything reinterprets
// its bytes.
struct ConfigRecordHeader {
    uint32_t magic;
    uint16_t schema;
    uint16_t size;  // sizeof(BoardConfig) when written
    uint32_t crc;   // CRC32 of the payload
};

constexpr uint32_t kConfigMagic = 0x4D454C4Fu;  // 'MELO'

struct ConfigRecord {
    ConfigRecordHeader header;
    BoardConfig config;
};

}  // namespace melodica

#endif  // CORE_CONFIGLEGACY_H
