#include "Calibration.h"
#include "KeyPulse.h"
#include "native_test.h"

using namespace melodica;

void run_keydriver_limits() {
    // A pressed pulse beyond the mechanical maximum must be clamped.
    KeyCalibration c;
    c.restPulseUs = 1500;
    c.pressedPulseUs = 3000;  // deliberately past maxPulseUs
    c.minPulseUs = 800;
    c.maxPulseUs = 2200;
    c.inverted = false;

    CHECK_EQ(keyTargetPulseUs(c, false), 1500);  // rest within limits
    CHECK_EQ(keyTargetPulseUs(c, true), 2200);   // pressed clamped to max

    // Rest below the minimum is clamped up.
    KeyCalibration c2;
    c2.restPulseUs = 400;
    c2.pressedPulseUs = 1200;
    c2.minPulseUs = 600;
    c2.maxPulseUs = 2400;
    CHECK_EQ(keyTargetPulseUs(c2, false), 600);

    // Inversion swaps rest/pressed.
    KeyCalibration c3;
    c3.restPulseUs = 1000;
    c3.pressedPulseUs = 2000;
    c3.minPulseUs = 500;
    c3.maxPulseUs = 2500;
    c3.inverted = true;
    CHECK_EQ(keyTargetPulseUs(c3, false), 2000);  // inverted rest = pressed value
    CHECK_EQ(keyTargetPulseUs(c3, true), 1000);

    // Pulse-to-ticks conversion is bounded to 12 bits.
    uint16_t ticks = pulseUsToTicks(1500, 50);  // 1.5ms @ 50Hz
    CHECK(ticks > 0 && ticks < 4096);
    CHECK(pulseUsToTicks(60000, 50) <= 4095);   // absurd input saturates
}

void run_calibration_migration() {
    // Legacy: rest angle 90deg, direction +1, press travel 20deg.
    LegacyServoCalibration old{90, 1};
    KeyCalibration k = migrateLegacyKey(old, 20);
    // 90deg -> ~1500us rest; pressed 70deg -> lower pulse.
    CHECK(k.restPulseUs > k.pressedPulseUs);
    CHECK(k.restPulseUs >= 1400 && k.restPulseUs <= 1600);
    CHECK_EQ(k.minPulseUs, 500);
    CHECK_EQ(k.maxPulseUs, 2500);

    // Reversed direction produces a pressed pulse above rest.
    LegacyServoCalibration oldRev{90, -1};
    KeyCalibration kr = migrateLegacyKey(oldRev, 20);
    CHECK(kr.pressedPulseUs > kr.restPulseUs);

    // Config CRC round-trips and validates.
    BoardConfig cfg;
    cfg.crc32 = computeConfigCrc(cfg);
    CHECK(validateConfig(cfg));

    // Corrupting a byte breaks validation.
    cfg.midi.transpose = 7;  // change after CRC computed
    CHECK(!validateConfig(cfg));
}
