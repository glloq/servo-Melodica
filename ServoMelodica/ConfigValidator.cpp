#include "ConfigValidator.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace melodica {

namespace {

// A GPIO claimed by one function, used to detect two jobs wired to one pin.
struct PinClaim {
    int8_t pin;
    const char* owner;
};

constexpr uint8_t kMaxClaims = 16;

class Report {
public:
    explicit Report(ConfigValidator::Result& r) : r_(r) {}

    void add(IssueSeverity sev, const char* fmt, ...) __attribute__((format(printf, 3, 4)));

private:
    ConfigValidator::Result& r_;
};

void Report::add(IssueSeverity sev, const char* fmt, ...) {
    if (r_.count >= ConfigValidator::kMaxIssues) {
        r_.truncated = true;
        if (sev == IssueSeverity::Error) ++r_.errors;
        return;
    }
    ConfigIssue& issue = r_.items[r_.count++];
    issue.severity = sev;
    va_list args;
    va_start(args, fmt);
    vsnprintf(issue.text, sizeof(issue.text), fmt, args);
    va_end(args);
    if (sev == IssueSeverity::Error) ++r_.errors; else ++r_.warnings;
}

}  // namespace

ConfigValidator::Result ConfigValidator::validate(const BoardConfig& cfg, ChipTarget target) {
    Result res;
    Report rep(res);

    PinClaim claims[kMaxClaims];
    uint8_t claimCount = 0;

    // Register a pin as used by `owner`, reporting conflicts and pin hazards.
    auto claim = [&](int8_t pin, const char* owner, bool needsOutput, bool needsAdc) {
        if (pin < 0) return;
        if (!GpioCaps::exists(pin, target)) {
            rep.add(IssueSeverity::Error, "GPIO%d does not exist on this chip (%s)", (int)pin,
                    owner);
            return;
        }
        if (GpioCaps::reservedForFlash(pin, target)) {
            rep.add(IssueSeverity::Error, "GPIO%d belongs to the SPI flash (%s)", (int)pin, owner);
            return;
        }
        if (needsOutput && GpioCaps::inputOnly(pin, target)) {
            rep.add(IssueSeverity::Error, "GPIO%d is input-only and cannot drive %s", (int)pin,
                    owner);
        }
        if (needsAdc && !GpioCaps::adcCapable(pin, target)) {
            rep.add(IssueSeverity::Error, "GPIO%d has no ADC (%s)", (int)pin, owner);
        }
        if (GpioCaps::usedByConsole(pin, target)) {
            rep.add(IssueSeverity::Warning, "GPIO%d is the serial console UART (%s)", (int)pin,
                    owner);
        } else if (GpioCaps::strapping(pin, target)) {
            rep.add(IssueSeverity::Warning, "GPIO%d is a boot strapping pin (%s)", (int)pin, owner);
        }
        for (uint8_t i = 0; i < claimCount; ++i) {
            if (claims[i].pin == pin) {
                rep.add(IssueSeverity::Error, "GPIO%d is used by both %s and %s", (int)pin,
                        claims[i].owner, owner);
                return;
            }
        }
        if (claimCount < kMaxClaims) claims[claimCount++] = PinClaim{pin, owner};
    };

    // --- Instrument geometry -------------------------------------------------
    const uint16_t keys = cfg.instrument.noteCount;
    if (keys == 0) {
        rep.add(IssueSeverity::Error, "The instrument needs at least one key");
    } else if (keys > MAX_NOTES) {
        rep.add(IssueSeverity::Error, "%u keys exceeds the %u-key maximum", (unsigned)keys,
                (unsigned)MAX_NOTES);
    } else {
        uint8_t needed = boardsNeeded(keys);
        if (cfg.keyDriver.boardCount < needed) {
            rep.add(IssueSeverity::Error, "%u keys need %u PCA9685 boards, %u configured",
                    (unsigned)keys, (unsigned)needed, (unsigned)cfg.keyDriver.boardCount);
        }
        uint16_t highest = static_cast<uint16_t>(cfg.instrument.firstMidiNote) + keys - 1;
        if (highest > 127) {
            rep.add(IssueSeverity::Error,
                    "Highest key would be MIDI %u (>127): lower the first note", highest);
        }
    }

    // --- I2C / PCA9685 -------------------------------------------------------
    claim(cfg.i2c.sdaPin, "I2C SDA", true, false);
    claim(cfg.i2c.sclPin, "I2C SCL", true, false);
    claim(cfg.keyDriver.oePin, "PCA9685 OE", true, false);
    if (cfg.i2c.clockHz < 50000u || cfg.i2c.clockHz > 1000000u) {
        rep.add(IssueSeverity::Warning, "I2C clock %u Hz is outside the usual 100k..400k",
                (unsigned)cfg.i2c.clockHz);
    }
    if (cfg.keyDriver.boardCount == 0 || cfg.keyDriver.boardCount > MAX_PCA_BOARDS) {
        rep.add(IssueSeverity::Error, "PCA9685 board count must be 1..%u",
                (unsigned)MAX_PCA_BOARDS);
    } else {
        for (uint8_t b = 0; b < cfg.keyDriver.boardCount; ++b) {
            uint8_t addr = cfg.keyDriver.addresses[b];
            if (addr < 0x40 || addr > 0x7F) {
                rep.add(IssueSeverity::Error, "PCA9685 #%u address 0x%02X outside 0x40..0x7F",
                        (unsigned)b, (unsigned)addr);
            }
            for (uint8_t o = 0; o < b; ++o) {
                if (cfg.keyDriver.addresses[o] == addr) {
                    rep.add(IssueSeverity::Error, "PCA9685 #%u and #%u share address 0x%02X",
                            (unsigned)o, (unsigned)b, (unsigned)addr);
                }
            }
        }
    }
    if (cfg.keyDriver.pwmFrequencyHz < 24 || cfg.keyDriver.pwmFrequencyHz > 1526) {
        rep.add(IssueSeverity::Error, "PCA9685 PWM frequency %u Hz outside 24..1526",
                (unsigned)cfg.keyDriver.pwmFrequencyHz);
    } else if (cfg.keyDriver.pwmFrequencyHz < 40 || cfg.keyDriver.pwmFrequencyHz > 400) {
        rep.add(IssueSeverity::Warning, "PCA9685 PWM frequency %u Hz is unusual for servos (50)",
                (unsigned)cfg.keyDriver.pwmFrequencyHz);
    }

    // --- Per-key calibration (active keys only) ------------------------------
    uint16_t activeKeys = keys > MAX_NOTES ? MAX_NOTES : keys;
    for (uint16_t i = 0; i < activeKeys; ++i) {
        const KeyCalibration& k = cfg.keys[i];
        if (k.minPulseUs >= k.maxPulseUs) {
            rep.add(IssueSeverity::Error, "Key %u: min pulse %u >= max pulse %u", (unsigned)i,
                    (unsigned)k.minPulseUs, (unsigned)k.maxPulseUs);
        } else if (k.restPulseUs < k.minPulseUs || k.restPulseUs > k.maxPulseUs ||
                   k.pressedPulseUs < k.minPulseUs || k.pressedPulseUs > k.maxPulseUs) {
            rep.add(IssueSeverity::Warning, "Key %u: rest/pressed outside %u..%u, will be clamped",
                    (unsigned)i, (unsigned)k.minPulseUs, (unsigned)k.maxPulseUs);
        } else if (k.restPulseUs == k.pressedPulseUs) {
            rep.add(IssueSeverity::Warning, "Key %u: rest equals pressed, it will never move",
                    (unsigned)i);
        }
    }

    // --- MIDI filter ---------------------------------------------------------
    if (cfg.midi.lowNote > cfg.midi.highNote) {
        rep.add(IssueSeverity::Error, "MIDI low note %u is above high note %u",
                (unsigned)cfg.midi.lowNote, (unsigned)cfg.midi.highNote);
    }
    if (cfg.midi.channelMode == ChannelMode::List && cfg.midi.channelMask == 0) {
        rep.add(IssueSeverity::Error, "Channel mode 'List' with no channel selected");
    }
    if (cfg.midi.channelMode == ChannelMode::Single && cfg.midi.singleChannel > 15) {
        rep.add(IssueSeverity::Error, "MIDI channel %u outside 1..16",
                (unsigned)(cfg.midi.singleChannel + 1));
    }

    // --- Air system ----------------------------------------------------------
    const AirConfig& a = cfg.air;
    const AirModuleWiring& w = a.wiring;
    if (a.minOutput > a.maxOutput) {
        rep.add(IssueSeverity::Error, "Air min output %u is above max output %u",
                (unsigned)a.minOutput, (unsigned)a.maxOutput);
    }
    if (a.pwmFrequencyHz == 0) {
        rep.add(IssueSeverity::Error, "Air PWM frequency must not be 0");
    }
    const uint8_t ledcMax = GpioCaps::ledcChannels(target);

    switch (a.type) {
        case AirSystemType::ServoValve:
            claim(w.primaryPin, "the air servo valve", true, false);
            break;
        case AirSystemType::Solenoid:
            claim(w.primaryPin, "the air solenoid", true, false);
            break;
        case AirSystemType::SteppedSolenoid:
            if (w.stageCount == 0 || w.stageCount > 4) {
                rep.add(IssueSeverity::Error, "Stepped solenoid stage count must be 1..4");
            }
            for (uint8_t i = 0; i < w.stageCount && i < 4; ++i) {
                claim(w.stagePins[i], "a solenoid stage", true, false);
            }
            break;
        case AirSystemType::BlowerPwm:
            claim(w.primaryPin, "the blower PWM", true, false);
            claim(w.enablePin, "the blower enable", true, false);
            if (w.ledcChannel >= ledcMax) {
                rep.add(IssueSeverity::Error, "LEDC channel %u outside 0..%u",
                        (unsigned)w.ledcChannel, (unsigned)(ledcMax - 1));
            }
            break;
        case AirSystemType::PumpHBridge:
            claim(w.primaryPin, "the pump PWM", true, false);
            claim(w.secondaryPin, "the pump direction", true, false);
            if (w.ledcChannel >= ledcMax) {
                rep.add(IssueSeverity::Error, "LEDC channel %u outside 0..%u",
                        (unsigned)w.ledcChannel, (unsigned)(ledcMax - 1));
            }
            break;
        case AirSystemType::PwmValve:
            claim(w.primaryPin, "the proportional valve", true, false);
            if (w.ledcChannel >= ledcMax) {
                rep.add(IssueSeverity::Error, "LEDC channel %u outside 0..%u",
                        (unsigned)w.ledcChannel, (unsigned)(ledcMax - 1));
            }
            break;
        case AirSystemType::PwmValveDac:
            claim(w.primaryPin, "the DAC valve", true, false);
            if (!GpioCaps::dacCapable(w.primaryPin, target)) {
                rep.add(IssueSeverity::Error, "GPIO%d has no DAC: use a PWM valve instead",
                        (int)w.primaryPin);
            }
            break;
        case AirSystemType::StepperBellows:
            claim(w.stepPin, "the stepper STEP", true, false);
            claim(w.dirPin, "the stepper DIR", true, false);
            if (w.stepperMaxSteps == 0) {
                rep.add(IssueSeverity::Error, "Stepper travel (max steps) must not be 0");
            }
            rep.add(IssueSeverity::Warning,
                    "Stepper bellows is open-loop: no homing and no end-stops");
            break;
        case AirSystemType::ExternalAir:
            claim(w.enablePin, "the external-air enable", true, false);
            break;
        case AirSystemType::Simulation:
            rep.add(IssueSeverity::Warning, "Air system is 'Simulation': no air will be produced");
            break;
        case AirSystemType::Composite: {
            const CompositeAirConfig& c = a.composite;
            if (c.mainValve == MainValveKind::Solenoid) {
                claim(c.mainValvePin, "the main valve", true, false);
            }
            switch (c.flow) {
                case FlowKind::Servo: claim(c.flowPin, "the flow servo", true, false); break;
                case FlowKind::Pwm:
                    claim(c.flowPin, "the flow PWM", true, false);
                    if (c.flowLedcChannel >= ledcMax) {
                        rep.add(IssueSeverity::Error, "Flow LEDC channel %u outside 0..%u",
                                (unsigned)c.flowLedcChannel, (unsigned)(ledcMax - 1));
                    }
                    break;
                case FlowKind::Dac:
                    claim(c.flowPin, "the flow DAC", true, false);
                    if (!GpioCaps::dacCapable(c.flowPin, target)) {
                        rep.add(IssueSeverity::Error, "GPIO%d has no DAC (flow stage)",
                                (int)c.flowPin);
                    }
                    break;
                case FlowKind::None:
                    if (c.mainValve == MainValveKind::None) {
                        rep.add(IssueSeverity::Error,
                                "Composite air with no main valve and no flow stage");
                    }
                    break;
            }
            bool sourceUsesPwm = c.source == SourceKind::Blower || c.source == SourceKind::Pump ||
                                 c.source == SourceKind::Reservoir;
            if (sourceUsesPwm) {
                claim(c.sourcePwmPin, "the air source PWM", true, false);
                if (c.sourceLedcChannel >= ledcMax) {
                    rep.add(IssueSeverity::Error, "Source LEDC channel %u outside 0..%u",
                            (unsigned)c.sourceLedcChannel, (unsigned)(ledcMax - 1));
                }
                bool flowUsesLedc = c.flow == FlowKind::Pwm;
                if (flowUsesLedc && c.flowLedcChannel == c.sourceLedcChannel) {
                    rep.add(IssueSeverity::Error, "Flow and source share LEDC channel %u",
                            (unsigned)c.sourceLedcChannel);
                }
            }
            if (c.source == SourceKind::Pump) claim(c.sourceDirPin, "the pump direction", true, false);
            if (c.source == SourceKind::External) claim(c.sourceEnablePin, "the external-air enable", true, false);

            if (c.sensorEnabled) {
                claim(c.sensorPin, "the pressure sensor", false, true);
                if (c.sensorMaxRaw <= c.sensorMinRaw) {
                    rep.add(IssueSeverity::Error,
                            "Pressure sensor raw max %u must be above min %u",
                            (unsigned)c.sensorMaxRaw, (unsigned)c.sensorMinRaw);
                }
                if (GpioCaps::adcSharedWithWifi(c.sensorPin, target)) {
                    rep.add(IssueSeverity::Warning,
                            "GPIO%d is on ADC2 and cannot be read while WiFi runs",
                            (int)c.sensorPin);
                }
                if (c.pressureHysteresis >= c.targetPressure && c.targetPressure > 0) {
                    rep.add(IssueSeverity::Warning,
                            "Pressure hysteresis %u >= target %u: the pump will rarely stop",
                            (unsigned)c.pressureHysteresis, (unsigned)c.targetPressure);
                }
            } else if (c.source == SourceKind::Reservoir) {
                rep.add(IssueSeverity::Error,
                        "Reservoir source needs a pressure sensor: refusing to run the pump");
            }
            break;
        }
    }

    // --- Safety --------------------------------------------------------------
    claim(cfg.safety.panicPin, "the emergency stop", false, false);
    if (cfg.safety.panicPin < 0) {
        rep.add(IssueSeverity::Warning,
                "No dedicated emergency-stop input (BOOT is a config button)");
    }
    if (cfg.safety.maxPumpRunMs == 0) {
        rep.add(IssueSeverity::Warning, "Pump overrun watchdog disabled (max pump run = 0)");
    }
    if (cfg.safety.maxKeyHoldMs == 0) {
        rep.add(IssueSeverity::Warning, "Stuck-key watchdog disabled (max key hold = 0)");
    }
    if (cfg.safety.commTimeoutMs == 0) {
        rep.add(IssueSeverity::Warning, "Communication watchdog disabled (comm timeout = 0)");
    }

    return res;
}

}  // namespace melodica
