#include <cstring>

#include "ConfigLegacy.h"
#include "native_test.h"

using namespace melodica;

// A stored v2 record must survive the upgrade to v3. Before the record header
// existed, ConfigStore required the blob to be exactly sizeof(BoardConfig), so
// the first schema change that resized the struct silently threw away every
// calibration the user had made.
void run_config_schema_migration() {
    legacy::BoardConfigV2 old{};
    old.schemaVersion = 2;
    old.instrument.noteCount = 37;
    old.instrument.firstMidiNote = 53;
    old.i2c.sdaPin = 18;
    old.i2c.sclPin = 19;
    old.i2c.clockHz = 100000;
    old.keyDriver.boardCount = 3;
    old.keyDriver.addresses[0] = 0x40;
    old.keyDriver.addresses[1] = 0x41;
    old.keyDriver.addresses[2] = 0x42;
    old.keyDriver.addresses[3] = 0x43;
    old.keyDriver.oePin = 4;
    old.keyDriver.pwmFrequencyHz = 50;
    old.keyDriver.slewUsPerMs = 7;
    old.air.type = AirSystemType::BlowerPwm;
    old.air.minOutput = 33;
    old.midi.channelMode = ChannelMode::List;
    old.midi.channelMask = 0x0005;
    std::strcpy(old.network.ssid, "atelier");
    std::strcpy(old.network.sessionName, "Melodica 1");
    old.safety.commTimeoutMs = 4000;
    old.safety.maxKeyHoldMs = 12000;
    old.safety.maxPumpRunMs = 60000;
    old.safety.disableOutputsOnIdle = true;
    old.safety.idleDisableDelayMs = 1500;
    for (uint16_t i = 0; i < MAX_NOTES; ++i) {
        old.keys[i].restPulseUs = static_cast<uint16_t>(1400 + i);
        old.keys[i].pressedPulseUs = static_cast<uint16_t>(1700 + i);
        old.keys[i].minPulseUs = 900;
        old.keys[i].maxPulseUs = 2100;
        old.keys[i].inverted = (i % 2) == 0;
    }
    old.crc32 = legacy::crcOfV2(old);
    CHECK(legacy::validV2(old));

    BoardConfig now = legacy::migrateV2(old);

    // Everything the operator configured survives.
    CHECK_EQ(now.schemaVersion, CONFIG_SCHEMA_VERSION);
    CHECK_EQ(now.instrument.noteCount, 37);
    CHECK_EQ(now.instrument.firstMidiNote, 53);
    CHECK_EQ(now.i2c.sdaPin, 18);
    CHECK_EQ(now.i2c.clockHz, 100000);
    CHECK_EQ(now.keyDriver.boardCount, 3);
    CHECK_EQ(now.keyDriver.oePin, 4);
    CHECK_EQ(now.keyDriver.slewUsPerMs, 7);
    CHECK_EQ(now.air.type == AirSystemType::BlowerPwm, true);
    CHECK_EQ(now.air.minOutput, 33);
    CHECK_EQ(now.midi.channelMask, 0x0005);
    CHECK(std::strcmp(now.network.ssid, "atelier") == 0);
    CHECK(std::strcmp(now.network.sessionName, "Melodica 1") == 0);
    CHECK_EQ(now.safety.maxPumpRunMs, 60000);
    CHECK_EQ(now.safety.idleDisableDelayMs, 1500);
    for (uint16_t i = 0; i < MAX_NOTES; ++i) {
        CHECK_EQ(now.keys[i].restPulseUs, 1400 + i);
        CHECK_EQ(now.keys[i].pressedPulseUs, 1700 + i);
        CHECK_EQ(now.keys[i].inverted, (i % 2) == 0);
    }

    // Fields introduced in v3 take their defaults rather than random bytes.
    BoardConfig fresh;
    CHECK_EQ(now.safety.activeSensingTimeoutMs, fresh.safety.activeSensingTimeoutMs);
    CHECK_EQ(now.safety.panicPin, fresh.safety.panicPin);
    CHECK_EQ(now.keyDriver.healthCheckIntervalMs, fresh.keyDriver.healthCheckIntervalMs);

    // And the migrated record validates under the current CRC.
    CHECK(validateConfig(now));

    // A corrupt v2 record is rejected instead of migrated.
    legacy::BoardConfigV2 bad = old;
    bad.instrument.noteCount = 12;  // changed after the CRC was computed
    CHECK(!legacy::validV2(bad));
}

// The record header is what makes the next migration possible at all.
void run_config_record_header() {
    CHECK_EQ(sizeof(ConfigRecord), sizeof(ConfigRecordHeader) + sizeof(BoardConfig));
    CHECK(sizeof(ConfigRecord) != sizeof(legacy::BoardConfigV2));  // lengths distinguish them
    CHECK_EQ(kConfigMagic, 0x4D454C4Fu);
}
