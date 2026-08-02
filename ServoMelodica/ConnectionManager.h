#ifndef CORE_CONNECTIONMANAGER_H
#define CORE_CONNECTIONMANAGER_H

#include <cstdint>

namespace melodica {

enum class LinkState : uint8_t { Disconnected, Connecting, Connected };

// Hardware/link abstraction so the reconnection logic can be unit-tested with a
// mock and reused by both the WiFi (RTP-MIDI) and BLE transports.
class ILink {
public:
    virtual ~ILink() = default;
    virtual void beginConnect() = 0;  // start an async, non-blocking connect
    virtual bool isUp() const = 0;    // current physical link status
};

// Non-blocking connection supervisor with exponential backoff. It NEVER spins or
// blocks: each update() does at most one state transition and returns. The main
// loop keeps servicing safety and actuators while the link is down, and the
// caller reacts to justLost()/justConnected() edges (e.g. allNotesOff on loss).
class ConnectionManager {
public:
    ConnectionManager(ILink& link, uint32_t baseDelayMs, uint32_t maxDelayMs)
        : link_(link), baseDelayMs_(baseDelayMs), maxDelayMs_(maxDelayMs) {}

    // Kick off the first connection attempt.
    void begin(uint32_t nowMs) {
        state_ = LinkState::Connecting;
        currentDelayMs_ = baseDelayMs_;
        retryAtMs_ = nowMs;  // attempt immediately
        link_.beginConnect();
    }

    // Advance the machine. Call every loop with millis(). Non-blocking.
    void update(uint32_t nowMs) {
        justLost_ = false;
        justConnected_ = false;
        bool up = link_.isUp();

        switch (state_) {
            case LinkState::Connecting:
                if (up) {
                    state_ = LinkState::Connected;
                    currentDelayMs_ = baseDelayMs_;  // reset backoff
                    justConnected_ = true;
                } else if (nowMs >= retryAtMs_) {
                    // Retry with growing backoff; still non-blocking.
                    scheduleRetry(nowMs);
                    link_.beginConnect();
                }
                break;

            case LinkState::Connected:
                if (!up) {
                    state_ = LinkState::Disconnected;
                    justLost_ = true;
                    scheduleRetry(nowMs);
                }
                break;

            case LinkState::Disconnected:
                if (up) {
                    state_ = LinkState::Connected;
                    currentDelayMs_ = baseDelayMs_;
                    justConnected_ = true;
                } else if (nowMs >= retryAtMs_) {
                    state_ = LinkState::Connecting;
                    link_.beginConnect();
                    scheduleRetry(nowMs);
                }
                break;
        }
    }

    LinkState state() const { return state_; }
    bool connected() const { return state_ == LinkState::Connected; }
    bool justLost() const { return justLost_; }
    bool justConnected() const { return justConnected_; }
    uint32_t currentBackoffMs() const { return currentDelayMs_; }

private:
    void scheduleRetry(uint32_t nowMs) {
        retryAtMs_ = nowMs + currentDelayMs_;
        uint32_t next = currentDelayMs_ * 2u;
        currentDelayMs_ = next > maxDelayMs_ ? maxDelayMs_ : next;
    }

    ILink& link_;
    uint32_t baseDelayMs_;
    uint32_t maxDelayMs_;
    uint32_t currentDelayMs_ = 0;
    uint32_t retryAtMs_ = 0;
    LinkState state_ = LinkState::Disconnected;
    bool justLost_ = false;
    bool justConnected_ = false;
};

}  // namespace melodica

#endif  // CORE_CONNECTIONMANAGER_H
