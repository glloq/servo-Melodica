#ifndef CORE_INSTRUMENTCONTROLLER_H
#define CORE_INSTRUMENTCONTROLLER_H

#include "AirDemand.h"
#include "Config.h"
#include "IAirController.h"
#include "IKeyDriver.h"
#include "MidiEvent.h"
#include "MidiFilter.h"
#include "NoteState.h"

namespace melodica {

// The transport- and actuator-independent heart of the instrument.
//
// It owns the note-state table, applies MIDI semantics (Note On/Off, CC7/11/64/
// 120/121/123, velocity-zero, multi-channel), computes the air demand from the
// currently sounding notes, and runs the non-blocking air/key synchronisation
// scheduler. It drives an IKeyDriver and an IAirController through their
// interfaces only — no BLE/WiFi/USB/Adafruit headers are included here.
//
// Multi-channel policy (documented & tested): a physical key is reference-counted
// across MIDI channels. It stays pressed while ANY channel holds a Note On for
// its note; it releases only when the last holding channel sends Note Off (or
// when sustain is released). The stored velocity is last-wins (the most recent
// Note On that pressed the key), which also drives the air demand for that key.
class InstrumentController {
public:
    InstrumentController(const BoardConfig& config, IKeyDriver& keys, IAirController& air);

    bool begin();

    // Feed one normalised MIDI event. Returns true if it was accepted (passed
    // the channel/note filter and mapped to a key or a handled controller).
    bool handleEvent(const MidiEvent& event);

    // Advance schedulers/ramps. Call every loop iteration with micros().
    void update(uint32_t nowUs);

    // Safety / lifecycle entry points.
    void allNotesOff(uint32_t nowUs);   // CC123: graceful release + air stop
    void allSoundOff(uint32_t nowUs);   // CC120: immediate hard stop
    void resetControllers();            // CC121: reset CC7/11/64 to defaults
    void panic(uint32_t nowUs);         // local emergency: hard stop everything
    void onTransportLost(uint32_t nowUs);  // link dropped: safe release + air off

    // Introspection (used by tests and the safety manager).
    const NoteState& note(uint16_t index) const { return notes_[index]; }
    const AirDemand& currentDemand() const { return demand_; }
    uint8_t activeKeyCount() const { return activeCount_; }
    uint16_t keyCount() const { return noteCount_; }
    MidiFilter& filter() { return filter_; }
    uint8_t volume() const { return volumeCc7_; }
    uint8_t expression() const { return expressionCc11_; }
    bool sustain() const { return sustainPedal_; }

private:
    int32_t keyIndexForNote(uint8_t midiNote) const;  // -1 if out of range
    void onNoteOn(uint16_t idx, uint8_t velocity, uint8_t channel, uint32_t nowUs);
    void onNoteOff(uint16_t idx, uint8_t channel, uint32_t nowUs);
    void scheduleKeyDown(uint16_t idx, uint32_t nowUs);
    void scheduleKeyUp(uint16_t idx, uint32_t nowUs);
    void recomputeAir(uint32_t nowUs);
    bool desired(uint16_t idx) const;  // key should be physically down

    const BoardConfig& config_;
    IKeyDriver& keys_;
    IAirController& air_;
    MidiFilter filter_;

    // Runtime geometry (arrays sized to the compile-time cap MAX_NOTES).
    const uint16_t noteCount_;
    const uint8_t firstMidiNote_;

    NoteState notes_[MAX_NOTES];
    uint16_t channelMask_[MAX_NOTES] = {0};  // per-note holding-channel bits

    // Per-key non-blocking scheduling.
    enum class KeyPhase : uint8_t { Idle, PendingPress, Down, PendingRelease };
    KeyPhase phase_[MAX_NOTES] = {KeyPhase::Idle};
    uint32_t dueUs_[MAX_NOTES] = {0};

    uint8_t volumeCc7_ = 127;
    uint8_t expressionCc11_ = 127;
    bool sustainPedal_ = false;

    uint8_t activeCount_ = 0;  // keys with desired()==true
    AirDemand demand_;
    bool wasSounding_ = false;    // true while notes were producing air
    bool airHoldActive_ = false;  // trailing air after last note (airReleaseDelay)
    uint32_t airHoldUntilUs_ = 0;
};

}  // namespace melodica

#endif  // CORE_INSTRUMENTCONTROLLER_H
