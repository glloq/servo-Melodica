#include "MidiLinkWatchdog.h"
#include "native_test.h"

using namespace melodica;

namespace {
constexpr uint16_t kTimeout = 300;
}

// The regression this file exists for: a five-second held note used to look like
// a disconnected cable, so the firmware released the key in the middle of it.
void run_link_watchdog_long_note() {
    MidiLinkWatchdog link;
    CHECK(!link.connected(0, kTimeout));  // UART not open yet

    link.setOpen(true);
    CHECK(link.connected(0, kTimeout));   // open = connected, even with no traffic

    // Note On, then five seconds of silence while the note is held.
    link.noteByte(0x90, 1000);
    link.noteByte(60, 1001);
    link.noteByte(100, 1002);
    CHECK(link.connected(1002, kTimeout));
    CHECK(link.connected(3000, kTimeout));
    CHECK(link.connected(6002, kTimeout));   // 5 s later: still connected
    CHECK(link.connected(600000, kTimeout)); // ten minutes of silence: still connected
    CHECK(!link.supervised());
}

void run_link_watchdog_active_sensing() {
    MidiLinkWatchdog link;
    link.setOpen(true);

    // The sender opts into supervision by emitting Active Sensing.
    link.noteByte(MidiLinkWatchdog::kActiveSensing, 1000);
    CHECK(link.supervised());
    CHECK(link.connected(1100, kTimeout));
    CHECK(link.connected(1300, kTimeout));   // exactly at the timeout: still up
    CHECK(!link.connected(1301, kTimeout));  // promise broken => link is down

    // Traffic resumes: the link comes back without needing a new 0xFE.
    link.noteByte(0x90, 5000);
    CHECK(link.connected(5100, kTimeout));
    CHECK(!link.connected(5400, kTimeout));

    // A zero timeout disables supervision entirely.
    CHECK(link.connected(999999, 0));
}

void run_link_watchdog_reset() {
    MidiLinkWatchdog link;
    link.setOpen(true);
    link.noteByte(MidiLinkWatchdog::kActiveSensing, 10);
    CHECK(link.supervised());
    CHECK(link.everReceived());
    link.reset();
    CHECK(!link.supervised());
    CHECK(!link.everReceived());
    CHECK(link.connected(999999, kTimeout));  // back to "silence proves nothing"
}
