#include "PanicInput.h"
#include "native_test.h"

using namespace melodica;

namespace {
constexpr uint16_t kDebounce = 5;
}

// The BOOT button only panics on RELEASE of a short press. An emergency stop has
// to fire while the operator is still pressing it.
void run_panic_input_fires_on_press() {
    PanicInput panic;

    // Resting state settles without firing.
    CHECK(!panic.update(100, false, kDebounce));
    CHECK(!panic.update(110, false, kDebounce));
    CHECK(!panic.active());

    // Button hit: fires as soon as the level has been stable for the debounce,
    // NOT when it is released.
    CHECK(!panic.update(200, true, kDebounce));   // first sighting, not settled
    CHECK(!panic.update(203, true, kDebounce));   // still inside the debounce
    CHECK(panic.update(206, true, kDebounce));    // settled => fire
    CHECK(panic.active());

    // Holding it does not re-fire.
    CHECK(!panic.update(500, true, kDebounce));
    CHECK(!panic.update(5000, true, kDebounce));

    // Release, then a second press fires again.
    CHECK(!panic.update(6000, false, kDebounce));
    CHECK(!panic.update(6010, false, kDebounce));
    CHECK(!panic.active());
    CHECK(!panic.update(7000, true, kDebounce));
    CHECK(panic.update(7010, true, kDebounce));
}

void run_panic_input_debounce() {
    PanicInput panic;
    panic.update(0, false, kDebounce);
    panic.update(20, false, kDebounce);

    // Contact bounce: never stable long enough, so nothing fires.
    CHECK(!panic.update(100, true, kDebounce));
    CHECK(!panic.update(102, false, kDebounce));
    CHECK(!panic.update(104, true, kDebounce));
    CHECK(!panic.update(106, false, kDebounce));
    CHECK(!panic.active());

    // A real press does.
    CHECK(!panic.update(200, true, kDebounce));
    CHECK(panic.update(210, true, kDebounce));
}

// A normally-closed loop that is already open at power-up (button latched down,
// or a cut wire) must be reported, not adopted as the resting state.
void run_panic_input_active_at_boot() {
    PanicInput panic;
    CHECK(!panic.update(0, true, kDebounce));
    CHECK(panic.update(10, true, kDebounce));
    CHECK(panic.active());
}
