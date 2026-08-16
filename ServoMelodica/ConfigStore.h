#ifndef PLATFORM_CONFIGSTORE_H
#define PLATFORM_CONFIGSTORE_H

#if defined(ARDUINO)

#include <Preferences.h>

#include "Calibration.h"
#include "Config.h"
#include "ConfigLegacy.h"
#include "Defaults.h"
#include "Log.h"

namespace melodica {

// Persists the whole BoardConfig blob in NVS (ESP32 Preferences) so calibration,
// WiFi credentials, preferred transport, air parameters, MIDI channel and
// transposition all survive a reboot and can be changed WITHOUT recompiling.
//
// Records are written as ConfigRecordHeader + BoardConfig: the header carries a
// magic, the schema version, the payload size and a CRC32, so a stored record is
// identified and length-checked before anything interprets it. That is what
// makes migration real — an older record is recognised by its own layout and
// converted field by field (see ConfigLegacy.h) instead of being thrown away for
// having the "wrong" size, which is what used to happen to every upgrade.
//
// Credentials live only here (NVS), never in the source tree.
class ConfigStore {
public:
    static constexpr const char* kNamespace = "melodica";
    static constexpr const char* kBlobKey = "cfg";

    // Load config from NVS, validating the header, size, schema and CRC, and
    // migrating older schemas. Returns defaults on any failure. Never blocks.
    static BoardConfig load() {
        BoardConfig defaults = withCrc(makeDefaultConfig());

        Preferences prefs;
        if (!prefs.begin(kNamespace, /*readOnly=*/true)) {
            LOG_WARN("NVS unavailable, using compiled defaults");
            return defaults;
        }
        size_t stored = prefs.getBytesLength(kBlobKey);

        if (stored == sizeof(ConfigRecord)) {
            ConfigRecord rec;
            size_t got = prefs.getBytes(kBlobKey, &rec, sizeof(rec));
            prefs.end();
            if (got != sizeof(rec)) return defaults;
            return fromRecord(rec, defaults);
        }

        if (stored == sizeof(legacy::BoardConfigV2)) {
            legacy::BoardConfigV2 old;
            size_t got = prefs.getBytes(kBlobKey, &old, sizeof(old));
            prefs.end();
            if (got == sizeof(old) && legacy::validV2(old)) {
                LOG_WARN("Config schema 2 found: migrating to %u",
                         (unsigned)CONFIG_SCHEMA_VERSION);
                BoardConfig migrated = legacy::migrateV2(old);
                save(migrated);  // rewrite in the new format so we migrate once
                return migrated;
            }
            LOG_ERROR("Schema 2 record is corrupt, using defaults");
            return defaults;
        }

        prefs.end();
        if (stored == 0) {
            LOG_INFO("No stored config, using defaults");
        } else {
            LOG_ERROR("Stored config has an unknown length (%u bytes), using defaults",
                      (unsigned)stored);
        }
        return defaults;
    }

    // Persist config (recomputing both CRCs). Returns false on NVS error.
    static bool save(BoardConfig cfg) {
        cfg.schemaVersion = CONFIG_SCHEMA_VERSION;
        cfg.crc32 = computeConfigCrc(cfg);

        ConfigRecord rec;
        rec.header.magic = kConfigMagic;
        rec.header.schema = CONFIG_SCHEMA_VERSION;
        rec.header.size = static_cast<uint16_t>(sizeof(BoardConfig));
        rec.config = cfg;
        rec.header.crc = payloadCrc(rec.config);

        Preferences prefs;
        if (!prefs.begin(kNamespace, /*readOnly=*/false)) return false;
        size_t put = prefs.putBytes(kBlobKey, &rec, sizeof(rec));
        prefs.end();
        return put == sizeof(rec);
    }

    // Wipe stored config (factory reset). Next load() returns defaults.
    static bool factoryReset() {
        Preferences prefs;
        if (!prefs.begin(kNamespace, false)) return false;
        prefs.clear();
        prefs.end();
        return true;
    }

private:
    static uint32_t payloadCrc(const BoardConfig& cfg) {
        return crc32Update(0, reinterpret_cast<const uint8_t*>(&cfg), sizeof(cfg));
    }

    static BoardConfig withCrc(BoardConfig cfg) {
        cfg.crc32 = computeConfigCrc(cfg);
        return cfg;
    }

    // Interpret a header-tagged record, migrating within the v3+ family.
    static BoardConfig fromRecord(const ConfigRecord& rec, const BoardConfig& defaults) {
        if (rec.header.magic != kConfigMagic) {
            LOG_ERROR("Config record has a bad magic, using defaults");
            return defaults;
        }
        if (rec.header.size != sizeof(BoardConfig)) {
            LOG_ERROR("Config payload is %u bytes, expected %u: using defaults",
                      (unsigned)rec.header.size, (unsigned)sizeof(BoardConfig));
            return defaults;
        }
        if (rec.header.crc != payloadCrc(rec.config)) {
            LOG_ERROR("Config CRC mismatch, using defaults");
            return defaults;
        }
        if (rec.header.schema > CONFIG_SCHEMA_VERSION) {
            LOG_ERROR("Config schema %u is newer than this firmware (%u), using defaults",
                      (unsigned)rec.header.schema, (unsigned)CONFIG_SCHEMA_VERSION);
            return defaults;
        }
        BoardConfig cfg = rec.config;
        if (rec.header.schema < CONFIG_SCHEMA_VERSION) {
            LOG_WARN("Config schema %u -> %u, migrating", (unsigned)rec.header.schema,
                     (unsigned)CONFIG_SCHEMA_VERSION);
            migrate(cfg, rec.header.schema);
            save(cfg);
        }
        LOG_INFO("Config loaded from NVS");
        return cfg;
    }

    // Forward-migrate a header-tagged record in place. Same-size schema bumps
    // land here; a bump that RESIZES BoardConfig needs a frozen struct and a
    // converter in ConfigLegacy.h instead (see legacy::migrateV2).
    static void migrate(BoardConfig& cfg, uint16_t fromSchema) {
        (void)fromSchema;  // no in-family migration steps yet
        cfg.schemaVersion = CONFIG_SCHEMA_VERSION;
        cfg.crc32 = computeConfigCrc(cfg);
    }
};

}  // namespace melodica

#endif  // ARDUINO
#endif  // PLATFORM_CONFIGSTORE_H
