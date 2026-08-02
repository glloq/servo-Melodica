#ifndef CORE_MIDIFILTER_H
#define CORE_MIDIFILTER_H

#include "Config.h"
#include "MidiEvent.h"

namespace melodica {

// Applies channel filtering, transposition and note-range limits before events
// reach the note logic. Kept separate from InstrumentController so it can be
// unit-tested in isolation and reconfigured at runtime.
class MidiFilter {
public:
    explicit MidiFilter(const MidiFilterConfig& config) : config_(config) {}

    void setConfig(const MidiFilterConfig& config) { config_ = config; }
    const MidiFilterConfig& config() const { return config_; }

    // True if `channel` (0..15) passes the channel filter.
    bool acceptsChannel(uint8_t channel) const;

    // For note events, applies transpose and range test. Returns false if the
    // (transposed) note should be rejected. On success `outNote` holds the
    // transposed note number. Non-note events should not call this.
    bool mapNote(uint8_t inNote, uint8_t& outNote) const;

private:
    MidiFilterConfig config_;
};

}  // namespace melodica

#endif  // CORE_MIDIFILTER_H
