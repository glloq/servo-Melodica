#include <cstring>

#include "ConfigValidator.h"
#include "native_test.h"

using namespace melodica;

namespace {

// A configuration that is valid on the reference board, used as the base for
// each single-fault case below.
BoardConfig goodConfig() {
    BoardConfig c;
    c.instrument.noteCount = 32;
    c.instrument.firstMidiNote = 65;
    c.keyDriver.boardCount = 2;
    c.i2c.sdaPin = 21;
    c.i2c.sclPin = 22;
    c.air.type = AirSystemType::ServoValve;
    c.air.wiring.primaryPin = 13;
    c.safety.panicPin = 27;
    return c;
}

bool hasText(const ConfigValidator::Result& r, const char* needle) {
    for (uint8_t i = 0; i < r.count; ++i) {
        if (std::strstr(r[i].text, needle) != nullptr) return true;
    }
    return false;
}

}  // namespace

void run_validator_accepts_reference_board() {
    ConfigValidator::Result r = ConfigValidator::validate(goodConfig());
    for (uint8_t i = 0; i < r.count; ++i) {
        if (r[i].severity == IssueSeverity::Error) {
            std::printf("  unexpected error: %s\n", r[i].text);
        }
    }
    CHECK(r.ok());
    CHECK_EQ(r.errors, 0);
}

void run_validator_geometry() {
    // 37 keys do not fit on two PCA9685 boards.
    BoardConfig c = goodConfig();
    c.instrument.noteCount = 37;
    ConfigValidator::Result r = ConfigValidator::validate(c);
    CHECK(!r.ok());
    CHECK(hasText(r, "need 3 PCA9685"));

    // Three boards make the same instrument legal.
    c.keyDriver.boardCount = 3;
    CHECK(ConfigValidator::validate(c).ok());

    // 64 keys from note 120 would run past MIDI 127.
    c = goodConfig();
    c.keyDriver.boardCount = 4;
    c.instrument.noteCount = 64;
    c.instrument.firstMidiNote = 120;
    r = ConfigValidator::validate(c);
    CHECK(!r.ok());
    CHECK(hasText(r, "MIDI 183"));

    // ...but 64 keys from note 36 are fine on four boards.
    c.instrument.firstMidiNote = 36;
    CHECK(ConfigValidator::validate(c).ok());

    CHECK_EQ(ConfigValidator::boardsNeeded(1), 1);
    CHECK_EQ(ConfigValidator::boardsNeeded(16), 1);
    CHECK_EQ(ConfigValidator::boardsNeeded(17), 2);
    CHECK_EQ(ConfigValidator::boardsNeeded(64), 4);
}

void run_validator_pins() {
    // GPIO34 is input-only: it cannot drive a servo valve.
    BoardConfig c = goodConfig();
    c.air.wiring.primaryPin = 34;
    ConfigValidator::Result r = ConfigValidator::validate(c);
    CHECK(!r.ok());
    CHECK(hasText(r, "input-only"));

    // Two jobs on one GPIO.
    c = goodConfig();
    c.air.wiring.primaryPin = 21;  // same as SDA
    r = ConfigValidator::validate(c);
    CHECK(!r.ok());
    CHECK(hasText(r, "used by both"));

    // SPI flash pins are never usable.
    c = goodConfig();
    c.air.wiring.primaryPin = 7;
    r = ConfigValidator::validate(c);
    CHECK(!r.ok());
    CHECK(hasText(r, "SPI flash"));

    // A strapping pin works but is worth a warning, not an error.
    c = goodConfig();
    c.air.wiring.primaryPin = 12;
    r = ConfigValidator::validate(c);
    CHECK(r.ok());
    CHECK(hasText(r, "strapping"));

    // DAC valve on a pin with no DAC.
    c = goodConfig();
    c.air.type = AirSystemType::PwmValveDac;
    c.air.wiring.primaryPin = 13;
    r = ConfigValidator::validate(c);
    CHECK(!r.ok());
    CHECK(hasText(r, "no DAC"));
    c.air.wiring.primaryPin = 25;  // real DAC pin on the classic ESP32
    CHECK(ConfigValidator::validate(c).ok());

    // The same DAC pin is not a DAC on the S3.
    CHECK(!ConfigValidator::validate(c, ChipTarget::Esp32S3).ok());
    // ...and GPIO34, input-only on the classic ESP32, is a normal output there.
    BoardConfig s3 = goodConfig();
    s3.i2c.sdaPin = 8;   // GPIO21..25 are not all bonded out on the S3
    s3.i2c.sclPin = 9;
    s3.air.wiring.primaryPin = 34;
    s3.safety.panicPin = 35;
    ConfigValidator::Result rs3 = ConfigValidator::validate(s3, ChipTarget::Esp32S3);
    for (uint8_t i = 0; i < rs3.count; ++i) {
        if (rs3[i].severity == IssueSeverity::Error) {
            std::printf("  unexpected S3 error: %s\n", rs3[i].text);
        }
    }
    CHECK(rs3.ok());
    // The same board definition is NOT valid on the classic ESP32.
    CHECK(!ConfigValidator::validate(s3, ChipTarget::Esp32).ok());
}

void run_validator_air_and_sensor() {
    // A regulated reservoir with no sensor is exactly the configuration that
    // would run the pump for ever on a permanent "pressure = 0".
    BoardConfig c = goodConfig();
    c.air.type = AirSystemType::Composite;
    c.air.composite.source = SourceKind::Reservoir;
    c.air.composite.sensorEnabled = false;
    ConfigValidator::Result r = ConfigValidator::validate(c);
    CHECK(!r.ok());
    CHECK(hasText(r, "needs a pressure sensor"));

    // Enabling a sensor with a degenerate range is just as bad.
    c.air.composite.sensorEnabled = true;
    c.air.composite.sensorPin = 34;  // ADC1, valid input
    c.air.composite.sensorMinRaw = 3000;
    c.air.composite.sensorMaxRaw = 3000;
    r = ConfigValidator::validate(c);
    CHECK(!r.ok());
    CHECK(hasText(r, "raw max"));

    // A sensible sensor passes.
    c.air.composite.sensorMinRaw = 0;
    c.air.composite.sensorMaxRaw = 4095;
    CHECK(ConfigValidator::validate(c).ok());

    // Flow and source cannot share an LEDC channel.
    c.air.composite.flow = FlowKind::Pwm;
    c.air.composite.flowLedcChannel = 5;
    c.air.composite.sourceLedcChannel = 5;
    r = ConfigValidator::validate(c);
    CHECK(!r.ok());
    CHECK(hasText(r, "share LEDC channel"));

    // Output floor above the ceiling.
    c = goodConfig();
    c.air.minOutput = 200;
    c.air.maxOutput = 100;
    r = ConfigValidator::validate(c);
    CHECK(!r.ok());
    CHECK(hasText(r, "min output"));
}

void run_validator_midi_and_safety() {
    // "List" mode with no channel selected accepts nothing at all.
    BoardConfig c = goodConfig();
    c.midi.channelMode = ChannelMode::List;
    c.midi.channelMask = 0;
    ConfigValidator::Result r = ConfigValidator::validate(c);
    CHECK(!r.ok());
    CHECK(hasText(r, "no channel selected"));

    c.midi.channelMask = 0x0003;
    CHECK(ConfigValidator::validate(c).ok());

    // Inverted note window.
    c = goodConfig();
    c.midi.lowNote = 100;
    c.midi.highNote = 50;
    CHECK(!ConfigValidator::validate(c).ok());

    // Disabled watchdogs and a missing emergency stop are warnings, not errors.
    c = goodConfig();
    c.safety.panicPin = -1;
    c.safety.maxPumpRunMs = 0;
    r = ConfigValidator::validate(c);
    CHECK(r.ok());
    CHECK(hasText(r, "emergency-stop"));
    CHECK(hasText(r, "Pump overrun watchdog disabled"));

    // Duplicate PCA9685 addresses.
    c = goodConfig();
    c.keyDriver.addresses[1] = c.keyDriver.addresses[0];
    r = ConfigValidator::validate(c);
    CHECK(!r.ok());
    CHECK(hasText(r, "share address"));

    // A broken per-key calibration is reported with its key number.
    c = goodConfig();
    c.keys[5].minPulseUs = 2000;
    c.keys[5].maxPulseUs = 1000;
    r = ConfigValidator::validate(c);
    CHECK(!r.ok());
    CHECK(hasText(r, "Key 5"));

    // A key beyond the configured count is not the operator's problem yet.
    c = goodConfig();
    c.instrument.noteCount = 4;
    c.keys[40].minPulseUs = 2000;
    c.keys[40].maxPulseUs = 1000;
    CHECK(ConfigValidator::validate(c).ok());
}
