#ifndef CORE_EVENTQUEUE_H
#define CORE_EVENTQUEUE_H

#include <cstdint>

#include "MidiEvent.h"

namespace melodica {

// Fixed-capacity single-producer/single-consumer ring buffer for MidiEvents.
// No dynamic allocation: transports decode into it from their callback/read
// path and the main loop drains it. On overflow the oldest event is dropped
// (a stuck note is worse than a lost one, and the drop is reported).
template <uint16_t Capacity>
class EventQueue {
public:
    bool push(const MidiEvent& e) {
        uint16_t next = advance(head_);
        if (next == tail_) {
            // Full: drop oldest to make room, keep newest.
            tail_ = advance(tail_);
            ++drops_;
        }
        buffer_[head_] = e;
        head_ = next;
        return true;
    }

    bool pop(MidiEvent& e) {
        if (tail_ == head_) return false;
        e = buffer_[tail_];
        tail_ = advance(tail_);
        return true;
    }

    bool empty() const { return tail_ == head_; }
    uint32_t drops() const { return drops_; }

private:
    static uint16_t advance(uint16_t i) { return static_cast<uint16_t>((i + 1) % Capacity); }
    MidiEvent buffer_[Capacity];
    uint16_t head_ = 0;
    uint16_t tail_ = 0;
    uint32_t drops_ = 0;
};

}  // namespace melodica

#endif  // CORE_EVENTQUEUE_H
