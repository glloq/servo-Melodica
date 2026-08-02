#include "MidiFilter.h"

namespace melodica {

bool MidiFilter::acceptsChannel(uint8_t channel) const {
    switch (config_.channelMode) {
        case ChannelMode::Omni:
            return true;
        case ChannelMode::Single:
            return channel == config_.singleChannel;
        case ChannelMode::List:
            return (config_.channelMask & (1u << (channel & 0x0F))) != 0;
    }
    return false;
}

bool MidiFilter::mapNote(uint8_t inNote, uint8_t& outNote) const {
    int transposed = static_cast<int>(inNote) + config_.transpose;
    if (transposed < 0 || transposed > 127) {
        return false;
    }
    if (transposed < config_.lowNote || transposed > config_.highNote) {
        return false;
    }
    outNote = static_cast<uint8_t>(transposed);
    return true;
}

}  // namespace melodica
