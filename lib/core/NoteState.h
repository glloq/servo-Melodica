#ifndef CORE_NOTESTATE_H
#define CORE_NOTESTATE_H

#include <cstdint>

namespace melodica {

// Per-note runtime state. One entry exists for every playable note of the
// instrument (indexed by servo/key index, not raw MIDI number).
//
//  keyPressed       : a Note On for this key is currently held by some channel.
//  physicallyActive : the key actuator is currently pressed down.
//  sustained        : the key is being held only because CC64 (sustain) is on.
//  velocity         : velocity of the note that currently owns this key.
//  channel          : channel that currently owns this key (see multi-channel
//                     policy in InstrumentController).
//  startedAtUs      : micros() timestamp when the note became active (used by
//                     the SafetyManager stuck-key watchdog).
struct NoteState {
    bool keyPressed = false;
    bool physicallyActive = false;
    bool sustained = false;
    uint8_t velocity = 0;
    uint8_t channel = 0;
    uint32_t startedAtUs = 0;
};

}  // namespace melodica

#endif  // CORE_NOTESTATE_H
