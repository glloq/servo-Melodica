#ifndef CORE_CONFIGVALIDATOR_H
#define CORE_CONFIGVALIDATOR_H

#include <cstdint>

#include "Config.h"
#include "HardwareCaps.h"
#include "PcaMapping.h"

namespace melodica {

// Checks a BoardConfig against physics and against the chip.
//
// The web UI accepts numbers; the hardware does not. Nothing used to stop an
// operator from asking for 64 keys on one PCA9685, a solenoid on the input-only
// GPIO34, a DAC valve on a pin that has no DAC, a regulated reservoir with no
// pressure sensor, or the same GPIO wired to two different jobs — the firmware
// would just save it, reboot, and half-work in a way that looks like a hardware
// fault.
//
// This validator is pure logic (no Arduino, no I/O) so the whole matrix is
// covered by the host tests, and it is used by both the web portal (which
// refuses to save a configuration with errors unless the operator explicitly
// forces it) and the serial console.
enum class IssueSeverity : uint8_t { Warning = 0, Error = 1 };

struct ConfigIssue {
    IssueSeverity severity = IssueSeverity::Warning;
    char text[72] = {0};
};

class ConfigValidator {
public:
    // Bounded on purpose: a Result is built on the stack inside HTTP handlers,
    // so it must stay small (~1.2 kB) next to a BoardConfig copy.
    static constexpr uint8_t kMaxIssues = 16;

    struct Result {
        ConfigIssue items[kMaxIssues];
        uint8_t count = 0;
        uint8_t errors = 0;
        uint8_t warnings = 0;
        bool truncated = false;  // more issues existed than kMaxIssues

        bool ok() const { return errors == 0; }
        const ConfigIssue& operator[](uint8_t i) const { return items[i]; }
    };

    // Validate everything. `target` defaults to the chip this firmware is built
    // for; the tests use it to check all variants.
    static Result validate(const BoardConfig& cfg, ChipTarget target = currentTarget());

    // How many PCA9685 boards `keys` keys require (same mapping the key driver
    // uses: board = key / 16).
    static constexpr uint8_t boardsNeeded(uint16_t keys) { return pcaBoardsRequired(keys); }
};

}  // namespace melodica

#endif  // CORE_CONFIGVALIDATOR_H
