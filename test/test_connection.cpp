#include "ConnectionManager.h"
#include "native_test.h"

using namespace melodica;

namespace {
// Simulated link whose up/down state and connect attempts are scriptable.
class MockLink : public ILink {
public:
    void beginConnect() override { ++connectCalls; }
    bool isUp() const override { return up; }
    bool up = false;
    int connectCalls = 0;
};
}  // namespace

// Models both BLE loss and WiFi loss + reconnection with exponential backoff.
void run_connection() {
    MockLink link;
    ConnectionManager cm(link, /*base*/ 500, /*max*/ 8000);

    cm.begin(0);
    CHECK_EQ(cm.state() == LinkState::Connecting, true);
    CHECK_EQ(link.connectCalls, 1);

    // Link comes up -> Connected, justConnected edge fires.
    link.up = true;
    cm.update(10);
    CHECK(cm.connected());
    CHECK(cm.justConnected());

    // --- Loss (BLE/WiFi drop) -> justLost edge, safe-stop trigger for caller.
    link.up = false;
    cm.update(20);
    CHECK(cm.justLost());
    CHECK_EQ(cm.state() == LinkState::Disconnected, true);

    // Backoff: no immediate retry; must wait for the scheduled time.
    int callsBefore = link.connectCalls;
    cm.update(21);  // too soon
    CHECK_EQ(link.connectCalls, callsBefore);

    // After the first backoff window, a retry is issued (non-blocking).
    cm.update(20 + 600);
    CHECK(link.connectCalls > callsBefore);
    CHECK_EQ(cm.state() == LinkState::Connecting, true);

    // Backoff grows across successive failed retries.
    uint32_t firstBackoff = cm.currentBackoffMs();
    cm.update(20 + 600 + firstBackoff + 1);
    CHECK(cm.currentBackoffMs() >= firstBackoff);  // doubled, capped at max

    // --- Reconnection: link returns -> Connected again.
    link.up = true;
    cm.update(20000);
    CHECK(cm.connected());
    CHECK(cm.justConnected());
    // Backoff resets after a successful reconnection.
    CHECK_EQ(cm.currentBackoffMs(), 500);
}
