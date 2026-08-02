#include "SafetyManager.h"

#include "Log.h"

namespace melodica {

namespace {
constexpr uint32_t msToUs(uint32_t ms) { return ms * 1000u; }
}  // namespace

SafetyManager::SafetyManager(const SafetyConfig& config, InstrumentController& instrument,
                             IKeyDriver& keys, IAirController& air)
    : config_(config), instrument_(instrument), keys_(keys), air_(air) {}

void SafetyManager::noteMidiActivity(uint32_t nowUs) {
    lastActivityUs_ = nowUs;
    activitySeen_ = true;
}

void SafetyManager::triggerPanic(uint32_t nowUs) {
    enterFault(SafetyFault::Panic, nowUs);
}

void SafetyManager::enterFault(SafetyFault fault, uint32_t nowUs) {
    if (fault_ == fault) return;  // already latched on this fault
    fault_ = fault;
    faultAtUs_ = nowUs;
    LOG_ERROR("Safety fault #%u -> forcing safe state", static_cast<unsigned>(fault));
    // 1) release keys, 2) close air, 3) disable PWM outputs, 4) latch code.
    instrument_.allSoundOff(nowUs);
    keys_.setOutputsEnabled(false);
    outputsDisabled_ = true;
}

void SafetyManager::update(uint32_t nowUs, bool connected) {
    if (connected) everConnected_ = true;

    // --- I2C health: a board that stopped answering is critical. ---
    if (!keys_.healthy()) {
        enterFault(SafetyFault::I2cFault, nowUs);
    }

    // --- Communication watchdog: no MIDI and no link. ---
    if (!connected && activitySeen_ &&
        (nowUs - lastActivityUs_) > msToUs(config_.commTimeoutMs)) {
        if (everConnected_ && instrument_.activeKeyCount() > 0) {
            enterFault(SafetyFault::TransportLost, nowUs);
        } else if (instrument_.activeKeyCount() > 0) {
            enterFault(SafetyFault::CommTimeout, nowUs);
        }
    }

    // --- Stuck-key watchdog. ---
    for (uint16_t i = 0; i < instrument_.keyCount(); ++i) {
        const NoteState& n = instrument_.note(i);
        if (n.physicallyActive &&
            (nowUs - n.startedAtUs) > msToUs(config_.maxKeyHoldMs)) {
            enterFault(SafetyFault::KeyStuck, nowUs);
            break;
        }
    }

    // --- Idle output shedding (drop OE after a quiet period to cut noise). ---
    if (fault_ == SafetyFault::None && config_.disableOutputsOnIdle) {
        bool idle = instrument_.activeKeyCount() == 0 &&
                    !instrument_.currentDemand().soundRequested;
        if (idle) {
            if (!idleTracking_) {
                idleTracking_ = true;
                idleSinceUs_ = nowUs;
            } else if (!outputsDisabled_ &&
                       (nowUs - idleSinceUs_) > msToUs(config_.idleDisableDelayMs)) {
                keys_.setOutputsEnabled(false);
                outputsDisabled_ = true;
            }
        } else {
            idleTracking_ = false;
            if (outputsDisabled_) {
                keys_.setOutputsEnabled(true);
                outputsDisabled_ = false;
            }
        }
    }

    // --- Recovery: only for transient faults whose cause has cleared, and never
    // on the same tick the fault latched. Stuck-key, pump overrun, air-not-init
    // and explicit panic require an operator/host acknowledgement (clearFault).
    if (fault_ != SafetyFault::None && nowUs != faultAtUs_) {
        bool recoverable = false;
        switch (fault_) {
            case SafetyFault::I2cFault:
                recoverable = keys_.healthy() && connected;
                break;
            case SafetyFault::TransportLost:
            case SafetyFault::CommTimeout:
                recoverable = connected;
                break;
            default:
                recoverable = false;  // manual clear required
                break;
        }
        if (recoverable) {
            LOG_INFO("Safety recovered from fault #%u", static_cast<unsigned>(fault_));
            fault_ = SafetyFault::None;
            keys_.setOutputsEnabled(true);
            outputsDisabled_ = false;
        }
    }
}

void SafetyManager::clearFault() {
    fault_ = SafetyFault::None;
    keys_.setOutputsEnabled(true);
    outputsDisabled_ = false;
}

const char* SafetyManager::faultText() const {
    switch (fault_) {
        case SafetyFault::None: return "OK";
        case SafetyFault::CommTimeout: return "COMM_TIMEOUT";
        case SafetyFault::TransportLost: return "TRANSPORT_LOST";
        case SafetyFault::I2cFault: return "I2C_FAULT";
        case SafetyFault::AirNotInitialised: return "AIR_NOT_INITIALISED";
        case SafetyFault::Panic: return "PANIC";
        case SafetyFault::KeyStuck: return "KEY_STUCK";
        case SafetyFault::PumpOverrun: return "PUMP_OVERRUN";
    }
    return "UNKNOWN";
}

}  // namespace melodica
