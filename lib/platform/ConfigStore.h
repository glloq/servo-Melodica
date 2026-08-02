#ifndef PLATFORM_CONFIGSTORE_H
#define PLATFORM_CONFIGSTORE_H

#if defined(ARDUINO)

#include <Preferences.h>

#include "Calibration.h"
#include "Config.h"
#include "Defaults.h"
#include "Log.h"

namespace melodica {

// Persists the whole BoardConfig blob in NVS (ESP32 Preferences) so calibration,
// WiFi credentials, preferred transport, air parameters, MIDI channel and
// transposition all survive a reboot and can be changed WITHOUT recompiling.
//
// Integrity is enforced by a schema version and a CRC32; a corrupt or
// out-of-date record falls back to compiled defaults. Credentials live only
// here (NVS), never in the source tree.
class ConfigStore {
public:
    static constexpr const char* kNamespace = "melodica";
    static constexpr const char* kBlobKey = "cfg";

    // Load config from NVS, validating schema + CRC. Returns defaults on any
    // failure. Never blocks, never loops.
    static BoardConfig load() {
        Preferences prefs;
        BoardConfig cfg = makeDefaultConfig();
        if (!prefs.begin(kNamespace, /*readOnly=*/true)) {
            LOG_WARN("NVS unavailable, using compiled defaults");
            return withCrc(cfg);
        }
        BoardConfig loaded;
        size_t got = prefs.getBytes(kBlobKey, &loaded, sizeof(loaded));
        prefs.end();
        if (got != sizeof(loaded)) {
            LOG_INFO("No stored config, using defaults");
            return withCrc(cfg);
        }
        if (!validateConfig(loaded)) {
            if (loaded.schemaVersion < CONFIG_SCHEMA_VERSION && loaded.schemaVersion > 0) {
                LOG_WARN("Config schema %u -> migrating", loaded.schemaVersion);
                migrate(loaded);
                if (validateConfig(loaded)) return loaded;
            }
            LOG_ERROR("Stored config invalid, using defaults");
            return withCrc(cfg);
        }
        LOG_INFO("Config loaded from NVS");
        return loaded;
    }

    // Persist config (recomputing the CRC). Returns false on NVS error.
    static bool save(BoardConfig cfg) {
        cfg.schemaVersion = CONFIG_SCHEMA_VERSION;
        cfg.crc32 = computeConfigCrc(cfg);
        Preferences prefs;
        if (!prefs.begin(kNamespace, /*readOnly=*/false)) return false;
        size_t put = prefs.putBytes(kBlobKey, &cfg, sizeof(cfg));
        prefs.end();
        return put == sizeof(cfg);
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
    static BoardConfig withCrc(BoardConfig cfg) {
        cfg.crc32 = computeConfigCrc(cfg);
        return cfg;
    }

    // Forward-migrate an older schema in place. Currently only v0->current, which
    // simply re-stamps the version; extend as the schema evolves.
    static void migrate(BoardConfig& cfg) {
        cfg.schemaVersion = CONFIG_SCHEMA_VERSION;
        cfg.crc32 = computeConfigCrc(cfg);
    }
};

}  // namespace melodica

#endif  // ARDUINO
#endif  // PLATFORM_CONFIGSTORE_H
