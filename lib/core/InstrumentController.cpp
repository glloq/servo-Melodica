#include "InstrumentController.h"

#include "AirPolicy.h"
#include "Log.h"

namespace melodica {

namespace {
constexpr uint32_t msToUs(uint16_t ms) { return static_cast<uint32_t>(ms) * 1000u; }
}  // namespace

InstrumentController::InstrumentController(const BoardConfig& config, IKeyDriver& keys,
                                          IAirController& air)
    : config_(config), keys_(keys), air_(air), filter_(config.midi) {}

bool InstrumentController::begin() {
    bool keysOk = keys_.begin();
    bool airOk = air_.begin();
    if (!keysOk) LOG_ERROR("Key driver failed to initialise");
    if (!airOk) LOG_ERROR("Air controller failed to initialise");
    // Start from a known-safe state.
    keys_.releaseAll();
    air_.stop();
    demand_ = AirDemand{};
    return keysOk && airOk;
}

int32_t InstrumentController::keyIndexForNote(uint8_t midiNote) const {
    if (midiNote < FIRST_MIDI_NOTE) return -1;
    uint16_t idx = static_cast<uint16_t>(midiNote - FIRST_MIDI_NOTE);
    if (idx >= NUMBER_OF_NOTES) return -1;
    return static_cast<int32_t>(idx);
}

bool InstrumentController::desired(uint16_t idx) const {
    return channelMask_[idx] != 0 || notes_[idx].sustained;
}

bool InstrumentController::handleEvent(const MidiEvent& event) {
    const uint32_t now = event.timestampUs;

    // Real-time / system messages that ignore channel filtering.
    switch (event.type) {
        case MidiEventType::SystemReset:
            panic(now);
            return true;
        case MidiEventType::Stop:
            // A sequencer Stop should silence the instrument safely.
            allNotesOff(now);
            return true;
        case MidiEventType::Clock:
        case MidiEventType::Start:
        case MidiEventType::Continue:
            return true;  // accepted, no action needed
        default:
            break;
    }

    if (!filter_.acceptsChannel(event.channel)) {
        return false;
    }

    switch (event.type) {
        case MidiEventType::NoteOn: {
            uint8_t mapped;
            if (!filter_.mapNote(event.data1, mapped)) return false;
            int32_t idx = keyIndexForNote(mapped);
            if (idx < 0) return false;
            if (event.data2 == 0) {
                onNoteOff(static_cast<uint16_t>(idx), event.channel, now);
            } else {
                onNoteOn(static_cast<uint16_t>(idx), event.data2, event.channel, now);
            }
            return true;
        }
        case MidiEventType::NoteOff: {
            uint8_t mapped;
            if (!filter_.mapNote(event.data1, mapped)) return false;
            int32_t idx = keyIndexForNote(mapped);
            if (idx < 0) return false;
            onNoteOff(static_cast<uint16_t>(idx), event.channel, now);
            return true;
        }
        case MidiEventType::ControlChange: {
            switch (static_cast<ControlChange>(event.data1)) {
                case ControlChange::Volume:
                    volumeCc7_ = event.data2;
                    recomputeAir(now);
                    return true;
                case ControlChange::Expression:
                    expressionCc11_ = event.data2;
                    recomputeAir(now);
                    return true;
                case ControlChange::Sustain: {
                    bool on = event.data2 >= 64;
                    if (sustainPedal_ && !on) {
                        // Pedal released: drop keys held only by sustain.
                        for (uint16_t i = 0; i < NUMBER_OF_NOTES; ++i) {
                            if (notes_[i].sustained && channelMask_[i] == 0) {
                                notes_[i].sustained = false;
                                notes_[i].keyPressed = false;
                                scheduleKeyUp(i, now);
                            }
                        }
                    }
                    sustainPedal_ = on;
                    recomputeAir(now);
                    return true;
                }
                case ControlChange::AllSoundOff:
                    allSoundOff(now);
                    return true;
                case ControlChange::ResetAllControllers:
                    resetControllers();
                    recomputeAir(now);
                    return true;
                case ControlChange::AllNotesOff:
                    allNotesOff(now);
                    return true;
                default:
                    return false;  // unhandled CC, silently ignored
            }
        }
        default:
            return false;  // PitchBend / ProgramChange / aftertouch: no-op here
    }
}

void InstrumentController::onNoteOn(uint16_t idx, uint8_t velocity, uint8_t channel,
                                    uint32_t nowUs) {
    bool wasDesired = desired(idx);
    channelMask_[idx] |= static_cast<uint16_t>(1u << (channel & 0x0F));
    notes_[idx].keyPressed = true;
    notes_[idx].sustained = false;  // an explicit press supersedes sustain hold
    notes_[idx].velocity = velocity;  // last-wins
    notes_[idx].channel = channel;
    if (!wasDesired) {
        notes_[idx].startedAtUs = nowUs;
        scheduleKeyDown(idx, nowUs);
    }
    recomputeAir(nowUs);
}

void InstrumentController::onNoteOff(uint16_t idx, uint8_t channel, uint32_t nowUs) {
    channelMask_[idx] &= static_cast<uint16_t>(~(1u << (channel & 0x0F)));
    if (channelMask_[idx] != 0) {
        return;  // still held by another channel
    }
    notes_[idx].keyPressed = false;
    if (sustainPedal_) {
        notes_[idx].sustained = true;  // keep down until pedal released
        return;
    }
    scheduleKeyUp(idx, nowUs);
    recomputeAir(nowUs);
}

void InstrumentController::scheduleKeyDown(uint16_t idx, uint32_t nowUs) {
    // Air leads the key: open air now (recomputeAir does this) and press after
    // the configured precharge + press delay, without blocking.
    uint32_t delay = msToUs(config_.air.airPrechargeMs) + msToUs(config_.air.keyPressDelayMs);
    if (phase_[idx] == KeyPhase::PendingRelease || phase_[idx] == KeyPhase::Down) {
        // Key is (or is about to remain) physically down: keep it down.
        phase_[idx] = KeyPhase::Down;
        notes_[idx].physicallyActive = true;
        return;
    }
    if (delay == 0) {
        keys_.pressKey(idx);
        notes_[idx].physicallyActive = true;
        phase_[idx] = KeyPhase::Down;
    } else {
        phase_[idx] = KeyPhase::PendingPress;
        dueUs_[idx] = nowUs + delay;
    }
}

void InstrumentController::scheduleKeyUp(uint16_t idx, uint32_t nowUs) {
    if (phase_[idx] == KeyPhase::PendingPress) {
        // Never physically pressed yet: cancel cleanly.
        phase_[idx] = KeyPhase::Idle;
        notes_[idx].physicallyActive = false;
        return;
    }
    uint32_t delay = msToUs(config_.air.keyReleaseDelayMs);
    if (delay == 0) {
        keys_.releaseKey(idx);
        notes_[idx].physicallyActive = false;
        phase_[idx] = KeyPhase::Idle;
    } else {
        phase_[idx] = KeyPhase::PendingRelease;
        dueUs_[idx] = nowUs + delay;
    }
}

void InstrumentController::recomputeAir(uint32_t nowUs) {
    uint8_t vel[NUMBER_OF_NOTES];
    uint8_t count = 0;
    uint8_t maxv = 0;
    for (uint16_t i = 0; i < NUMBER_OF_NOTES; ++i) {
        if (desired(i)) {
            vel[count++] = notes_[i].velocity;
            if (notes_[i].velocity > maxv) maxv = notes_[i].velocity;
        }
    }
    activeCount_ = count;

    bool forcedSilence = (volumeCc7_ == 0 || expressionCc11_ == 0);
    uint8_t expr = computeAirExpression(config_.air.policy, vel, count, volumeCc7_,
                                        expressionCc11_);

    if (count > 0 && !forcedSilence && expr > 0) {
        demand_.expression = expr;
        demand_.maxVelocity = maxv;
        demand_.activeNoteCount = count;
        demand_.soundRequested = true;
        wasSounding_ = true;
        airHoldActive_ = false;
    } else if (!forcedSilence && wasSounding_ && count == 0 &&
               config_.air.airReleaseDelayMs > 0) {
        // Notes ended naturally: hold air briefly before closing (trailing air).
        if (!airHoldActive_) {
            airHoldActive_ = true;
            airHoldUntilUs_ = nowUs + msToUs(config_.air.airReleaseDelayMs);
        }
        if (static_cast<int32_t>(nowUs - airHoldUntilUs_) < 0) {
            demand_.activeNoteCount = 0;
            demand_.soundRequested = true;  // keep last expression flowing
        } else {
            demand_ = AirDemand{};
            airHoldActive_ = false;
            wasSounding_ = false;
        }
    } else {
        // Forced silence (CC7/CC11 == 0) or nothing sounding: close immediately.
        demand_ = AirDemand{};
        airHoldActive_ = false;
        wasSounding_ = false;
    }
    air_.setDemand(demand_);
}

void InstrumentController::update(uint32_t nowUs) {
    for (uint16_t i = 0; i < NUMBER_OF_NOTES; ++i) {
        // Wrap-safe deadline test: (int32_t)(now - due) >= 0 stays correct across
        // the ~71-minute micros() rollover.
        bool due = static_cast<int32_t>(nowUs - dueUs_[i]) >= 0;
        if (phase_[i] == KeyPhase::PendingPress && due) {
            keys_.pressKey(i);
            notes_[i].physicallyActive = true;
            phase_[i] = KeyPhase::Down;
        } else if (phase_[i] == KeyPhase::PendingRelease && due) {
            keys_.releaseKey(i);
            notes_[i].physicallyActive = false;
            phase_[i] = KeyPhase::Idle;
        }
    }
    // Expire the trailing-air hold and refresh CC-driven demand.
    if (airHoldActive_) {
        recomputeAir(nowUs);
    }
    keys_.update(nowUs);
    air_.update(nowUs);
}

void InstrumentController::allNotesOff(uint32_t nowUs) {
    (void)nowUs;
    LOG_INFO("All Notes Off");
    for (uint16_t i = 0; i < NUMBER_OF_NOTES; ++i) {
        channelMask_[i] = 0;
        notes_[i].keyPressed = false;
        notes_[i].sustained = false;
        notes_[i].physicallyActive = false;
        phase_[i] = KeyPhase::Idle;
    }
    sustainPedal_ = false;
    keys_.releaseAll();
    demand_ = AirDemand{};
    wasSounding_ = false;
    airHoldActive_ = false;
    activeCount_ = 0;
    air_.stop();
}

void InstrumentController::allSoundOff(uint32_t nowUs) {
    (void)nowUs;
    LOG_INFO("All Sound Off");
    for (uint16_t i = 0; i < NUMBER_OF_NOTES; ++i) {
        channelMask_[i] = 0;
        notes_[i].keyPressed = false;
        notes_[i].sustained = false;
        notes_[i].physicallyActive = false;
        phase_[i] = KeyPhase::Idle;
    }
    sustainPedal_ = false;
    keys_.releaseAll();
    demand_ = AirDemand{};
    wasSounding_ = false;
    airHoldActive_ = false;
    activeCount_ = 0;
    air_.emergencyStop();
}

void InstrumentController::resetControllers() {
    LOG_INFO("Reset All Controllers");
    volumeCc7_ = 127;
    expressionCc11_ = 127;
    sustainPedal_ = false;
}

void InstrumentController::panic(uint32_t nowUs) {
    LOG_WARN("PANIC: emergency stop");
    allSoundOff(nowUs);
}

void InstrumentController::onTransportLost(uint32_t nowUs) {
    LOG_WARN("Transport lost: safe stop");
    allNotesOff(nowUs);
}

}  // namespace melodica
