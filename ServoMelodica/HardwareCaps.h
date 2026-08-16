#ifndef CORE_HARDWARECAPS_H
#define CORE_HARDWARECAPS_H

#include <cstdint>

namespace melodica {

// What a given GPIO can actually do, per ESP32 variant.
//
// The configuration UI lets the operator type any pin number into any field, so
// something has to know that GPIO34 can never drive a solenoid, that GPIO6..11
// belong to the SPI flash, and that only GPIO25/26 have a DAC on the classic
// ESP32. That knowledge lives here, as pure functions, so both ConfigValidator
// and the air modules can refuse an impossible wiring instead of silently
// producing no movement (or crashing the chip).
enum class ChipTarget : uint8_t { Esp32, Esp32S2, Esp32S3 };

// The variant this firmware was compiled for (host tests default to the classic
// ESP32, which is also the reference board of the project).
constexpr ChipTarget currentTarget() {
#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(ARDUINO_ESP32S3_DEV)
    return ChipTarget::Esp32S3;
#elif defined(CONFIG_IDF_TARGET_ESP32S2) || defined(ARDUINO_ESP32S2_DEV)
    return ChipTarget::Esp32S2;
#else
    return ChipTarget::Esp32;
#endif
}

class GpioCaps {
public:
    // The pad physically exists on this package.
    static bool exists(int pin, ChipTarget t = currentTarget()) {
        if (pin < 0) return false;
        switch (t) {
            case ChipTarget::Esp32:
                // 20, 24, 28..31 are not bonded out on the classic ESP32.
                return pin <= 39 && pin != 20 && pin != 24 && !(pin >= 28 && pin <= 31);
            case ChipTarget::Esp32S2:
                return pin <= 46 && !(pin >= 22 && pin <= 25) && pin != 47 && pin != 48;
            case ChipTarget::Esp32S3:
                return pin <= 48 && !(pin >= 22 && pin <= 25);
        }
        return false;
    }

    // Reserved by the on-package SPI flash (and PSRAM): never usable.
    static bool reservedForFlash(int pin, ChipTarget t = currentTarget()) {
        switch (t) {
            case ChipTarget::Esp32: return pin >= 6 && pin <= 11;
            case ChipTarget::Esp32S2: return pin >= 26 && pin <= 32;
            case ChipTarget::Esp32S3: return pin >= 26 && pin <= 32;
        }
        return false;
    }

    // Input-only pads (no output driver at all): classic ESP32 GPIO34..39.
    static bool inputOnly(int pin, ChipTarget t = currentTarget()) {
        switch (t) {
            case ChipTarget::Esp32: return pin >= 34 && pin <= 39;
            case ChipTarget::Esp32S2: return pin == 46;
            case ChipTarget::Esp32S3: return false;
        }
        return false;
    }

    // Can drive a servo / solenoid / PWM output.
    static bool outputCapable(int pin, ChipTarget t = currentTarget()) {
        return exists(pin, t) && !reservedForFlash(pin, t) && !inputOnly(pin, t);
    }

    // Has an ADC channel (pressure sensor input).
    static bool adcCapable(int pin, ChipTarget t = currentTarget()) {
        if (!exists(pin, t) || reservedForFlash(pin, t)) return false;
        switch (t) {
            case ChipTarget::Esp32:
                // ADC1: 32..39. ADC2: 0,2,4,12..15,25..27 (unusable while WiFi runs).
                return (pin >= 32 && pin <= 39) || pin == 0 || pin == 2 || pin == 4 ||
                       (pin >= 12 && pin <= 15) || (pin >= 25 && pin <= 27);
            case ChipTarget::Esp32S2:
            case ChipTarget::Esp32S3:
                return pin >= 1 && pin <= 20;  // ADC1 1..10, ADC2 11..20
        }
        return false;
    }

    // ADC2 cannot be read while WiFi is active — a real trap for a pressure
    // sensor on a WiFi build.
    static bool adcSharedWithWifi(int pin, ChipTarget t = currentTarget()) {
        if (!adcCapable(pin, t)) return false;
        switch (t) {
            case ChipTarget::Esp32:
                return pin == 0 || pin == 2 || pin == 4 || (pin >= 12 && pin <= 15) ||
                       (pin >= 25 && pin <= 27);
            case ChipTarget::Esp32S2:
            case ChipTarget::Esp32S3:
                return pin >= 11 && pin <= 20;
        }
        return false;
    }

    // True analogue output (DAC): classic ESP32 GPIO25/26, S2 GPIO17/18, none on S3.
    static bool dacCapable(int pin, ChipTarget t = currentTarget()) {
        switch (t) {
            case ChipTarget::Esp32: return pin == 25 || pin == 26;
            case ChipTarget::Esp32S2: return pin == 17 || pin == 18;
            case ChipTarget::Esp32S3: return false;
        }
        return false;
    }

    // Boot/strapping pins: usable, but a pull from an actuator can stop the board
    // from booting. Worth a warning, not an error.
    static bool strapping(int pin, ChipTarget t = currentTarget()) {
        switch (t) {
            case ChipTarget::Esp32:
                return pin == 0 || pin == 2 || pin == 5 || pin == 12 || pin == 15;
            case ChipTarget::Esp32S2: return pin == 0 || pin == 45 || pin == 46;
            case ChipTarget::Esp32S3: return pin == 0 || pin == 3 || pin == 45 || pin == 46;
        }
        return false;
    }

    // UART0 = the USB/serial console and the calibration console.
    static bool usedByConsole(int pin, ChipTarget t = currentTarget()) {
        switch (t) {
            case ChipTarget::Esp32: return pin == 1 || pin == 3;
            case ChipTarget::Esp32S2:
            case ChipTarget::Esp32S3: return pin == 43 || pin == 44;
        }
        return false;
    }

    // Number of independent LEDC channels available for PWM actuators.
    static uint8_t ledcChannels(ChipTarget t = currentTarget()) {
        return t == ChipTarget::Esp32 ? 16 : 8;
    }
};

}  // namespace melodica

#endif  // CORE_HARDWARECAPS_H
