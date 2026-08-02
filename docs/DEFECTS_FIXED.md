# Corrected defects

Every mandatory correction from the brief, with where it is enforced.

| # | Defect (old behaviour) | Correction | Where |
|---|------------------------|-----------|-------|
| 1 | PCA9685 treated as 15 channels (`PWM_CHANNELS_PER_DRIVER 15`) | 16 channels per board | `lib/core/PcaMapping.h` (`CHANNELS_PER_PCA9685 = 16`) |
| 2 | Servo split used `servoNum < 15` / `servoNum - 15` | `driverIndex = i/16`, `channel = i%16` | `pcaLocationForServo()`; tested for servos 0/15/16/31 |
| 3 | Two PCAs hard-coded | Configurable board count + address list | `KeyDriverConfig.boardCount/addresses`, `MAX_PCA_BOARDS` |
| 4 | Mixed-case `#include "Instrument.h"` vs `instrument.h` | Single canonical casing everywhere | all headers use exact-case includes (`InstrumentController.h`, …) |
| 5 | BLE and WiFi each duplicated `instrument.cpp`/`ServoController.cpp` | One shared core | `lib/core/InstrumentController` used by all profiles |
| 6 | Blocking `delay()` in servo reset and setup | No blocking `delay()` in the run loop | non-blocking schedulers; `delay` only in the calibration console where no MIDI flows |
| 7 | `while(1){delay}` on WiFi/hardware failure | Non-blocking state machines, no infinite loops | `ConnectionManager`, `SafetyManager`, `main.cpp` |
| 8 | OE pin handling absent/wrong | Active-low OE driven correctly (LOW = enabled) | `Pca9685KeyDriver::setOutputsEnabled()` |
| 9 | OE described as "cut servo power" | Documented as output-disable (tri-state), not a power cut | code comments + `docs/WIRING.md` |
| 10 | `CC7 = 0` not handled (map from velocity 1) | `CC7 = 0` fully closes the air | `computeAirExpression()`; tested |
| 11 | Air only recomputed on note-on; never lowered | Air recomputed on every note change, including release | `InstrumentController::recomputeAir()`; tested |
| 12 | Velocity not stored per note | `NoteState.velocity` per active note | `lib/core/NoteState.h` |
| 13 | No per-servo mechanical limits | Per-key `minPulseUs`/`maxPulseUs` clamp | `KeyCalibration` + `keyTargetPulseUs()`; tested |
| 14 | WiFi SSID/password committed in `settings.h` | Credentials only in NVS; git-ignored | `NetworkConfig` (NVS), `.gitignore`, `docs/WIFI.md` |
| 15 | `new Instrument()` heap allocation | Static composition, no heap in real-time paths | `src/main.cpp` (function-local statics) |

## Additional structural fixes

- Replaced `AIR_ANTICIPATION_MS` with a real non-blocking scheduler
  (`airPrechargeMs`, `keyPressDelayMs`, `keyReleaseDelayMs`, `airReleaseDelayMs`).
- Removed the misleading "16 notes / 30 servos / 32 array" inconsistency;
  standardised on 32 keys and documented the octave convention.
- Incomplete/announced-but-unimplemented features (audio auto-calibration, pitch
  bend / modulation stubs that only logged) were removed from the documentation
  or replaced with working equivalents, so the docs match the code.
- Logging now has `ERROR/WARN/INFO/DEBUG` levels selectable at compile time.
