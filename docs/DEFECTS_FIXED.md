# Corrected defects

Every mandatory correction from the brief, with where it is enforced.

| # | Defect (old behaviour) | Correction | Where |
|---|------------------------|-----------|-------|
| 1 | PCA9685 treated as 15 channels (`PWM_CHANNELS_PER_DRIVER 15`) | 16 channels per board | `PcaMapping.h` (`CHANNELS_PER_PCA9685 = 16`) |
| 2 | Servo split used `servoNum < 15` / `servoNum - 15` | `driverIndex = i/16`, `channel = i%16` | `pcaLocationForServo()`; tested for servos 0/15/16/31 |
| 3 | Two PCAs hard-coded | Configurable board count + address list | `KeyDriverConfig.boardCount/addresses`, `MAX_PCA_BOARDS` |
| 4 | Mixed-case `#include "Instrument.h"` vs `instrument.h` | Single canonical casing everywhere | all headers use exact-case includes (`InstrumentController.h`, …) |
| 5 | BLE and WiFi each duplicated `instrument.cpp`/`ServoController.cpp` | One shared core | `InstrumentController` used by all profiles |
| 6 | Blocking `delay()` in servo reset and setup | No blocking `delay()` in the run loop | non-blocking schedulers; `delay` only in the calibration console where no MIDI flows |
| 7 | `while(1){delay}` on WiFi/hardware failure | Non-blocking state machines, no infinite loops | `ConnectionManager`, `SafetyManager`, `main.cpp` |
| 8 | OE pin handling absent/wrong | Active-low OE driven correctly (LOW = enabled) | `Pca9685KeyDriver::setOutputsEnabled()` |
| 9 | OE described as "cut servo power" | Documented as output-disable (tri-state), not a power cut | code comments + `docs/WIRING.md` |
| 10 | `CC7 = 0` not handled (map from velocity 1) | `CC7 = 0` fully closes the air | `computeAirExpression()`; tested |
| 11 | Air only recomputed on note-on; never lowered | Air recomputed on every note change, including release | `InstrumentController::recomputeAir()`; tested |
| 12 | Velocity not stored per note | `NoteState.velocity` per active note | `NoteState.h` |
| 13 | No per-servo mechanical limits | Per-key `minPulseUs`/`maxPulseUs` clamp | `KeyCalibration` + `keyTargetPulseUs()`; tested |
| 14 | WiFi SSID/password committed in `settings.h` | Credentials only in NVS; git-ignored | `NetworkConfig` (NVS), `.gitignore`, `docs/WIFI.md` |
| 15 | `new Instrument()` heap allocation | Static composition, no heap in real-time paths | `ServoMelodica.ino` (function-local statics) |

## Reliability pass (hardware, transport, safety, UI audit)

A later audit went through the hardware, transport, safety and UI layers. What it
found, and what each fix does:

| # | Defect | Correction | Where |
|---|--------|-----------|-------|
| 16 | DIN reported "disconnected" after 2 s of silence, so **any note held longer than 2 s was released mid-note** (`onTransportLost` → `allNotesOff`) | Silence no longer means loss. The link is up while the UART is open; supervision is armed only by Active Sensing (0xFE), the sender's own promise to keep talking | `MidiLinkWatchdog`, `DinMidiTransport`; `test_link_watchdog.cpp` |
| 17 | Serial-from-Pi had the same false timeout at 3 s | Same watchdog. A host bridge opts into supervision by sending 0xFE | `SerialMidiTransport` |
| 18 | `UsbMidiTransport::connected()` always returned `true`: unplugging the cable left keys pressed until the 30 s stuck-key watchdog | Reports the real TinyUSB state (`mounted() && !suspended()`), and stops pumping MIDI while unplugged | `UsbMidiTransport` |
| 19 | `maxPumpRunMs` and the `PumpOverrun` fault existed in the config and the enum, but nothing ever measured a pump's run time | Air modules track their own uninterrupted run time; `SafetyManager` enforces the limit and latches `PumpOverrun` | `AirRunTracker`, `AirRamp`, `IAirController`, `SafetyManager`; `test_air_runtime.cpp` |
| 20 | A reservoir with a missing or mis-calibrated sensor read "0 bar" for ever and ran its pump indefinitely | `CompositeAirController` refuses to start a regulated reservoir without a valid sensor, faults, and never drives its stages | `AirStages.h`, `AnalogPressureSensor::valid()`; `test_composite_air.cpp` |
| 21 | An air module with an unusable pin (input-only, SPI-flash, no DAC) was driven anyway | Pins are checked in `begin()`; a bad module latches `AirFault` and stays silent, which `SafetyManager` turns into `AIR_NOT_INITIALISED` | `HardwareCaps`, `HardwareAirControllers`, `AirStages_ESP32` |
| 22 | Calibration was bounded by the compiled `NUMBER_OF_NOTES = 32`, so **keys 32–63 of a 64-key build could not be calibrated** (web UI, console, export, test-all) | Everything key-indexed is bounded by `instrument.noteCount` | `WebConfigPortal`, `CalibrationConsole` |
| 23 | The web calibration panel showed hard-coded 1500/1900 for every key: saving key 12 overwrote its real calibration | `GET /api/key?index=N` returns the stored values, and the UI reloads them whenever the key selector changes | `WebConfigPortal` |
| 24 | The config portal set `WIFI_AP_STA` while the RTP transport's retries called `WiFi.mode(WIFI_STA)`, tearing the setup hotspot down under the operator | One owner of the WiFi mode; the transport only asks for a station, the portal only asks for an access point | `NetworkManager`, `WifiLink` |
| 25 | `network.sessionName` was editable but unused — the AppleMIDI session kept its compiled name | Applied at session start when the library exposes a setter, and logged when it does not | `RtpMidiTransport` |
| 26 | The BOOT button only panicked on **release** of a short press, and did nothing at all between 1.5 s and 3 s | BOOT stays a configuration control; a dedicated `safety.panicPin` fires on the transition into the active state, debounced, with a normally-closed wiring so a cut wire also stops the instrument | `PanicInput`, `ServoMelodica.ino`; `test_panic.cpp` |
| 27 | `Pca9685KeyDriver::healthy()` only reflected the boot-time I2C probe: a board that died later stayed "healthy" for ever | One board is re-probed per interval (round-robin); a board that returns is re-initialised and its keys restored | `Pca9685KeyDriver` |
| 28 | `ChannelMode::List` was unusable: `channelMask` was never exposed, and its default `0xFFFF` made List identical to Omni | 16 per-channel checkboxes in the web UI | `WebConfigPortal` |
| 29 | The UI accepted physically impossible configurations (64 keys on one PCA9685, solenoid on an input-only pin, DAC on a non-DAC pin, one GPIO for two jobs, reservoir with no sensor…) | `ConfigValidator` returns errors and warnings; saving is refused unless explicitly forced, and every boot logs the issues | `ConfigValidator`, `HardwareCaps`; `test_validator.cpp` |
| 30 | `ConfigStore::load()` required the blob to be exactly `sizeof(BoardConfig)` **before** considering migration, so any schema change that resized the struct wiped every stored calibration | Records carry a header (magic, schema, size, CRC); older layouts are frozen in `ConfigLegacy.h` and migrated field by field | `ConfigStore`, `ConfigLegacy`; `test_config_schema.cpp` |
| 31 | In composite air, the attack/release ramp reached the flow stage only: with `Flow = None` + `Source = Blower` the envelope had almost no effect | The shaped level drives the source too (pre-inversion, so an inverted flow valve does not invert the blower) | `CompositeAirController` |
| 32 | Settings that existed only in `Config.h`: servo slew rate, I2C clock, PCA PWM frequency, LEDC channel/resolution, solenoid stage pins, reconnect backoff, the whole `SafetyConfig` | All exposed in the web UI | `WebConfigPortal` |
| 33 | No status feedback: the operator could not see why the instrument had stopped | `/api/status` + a live dashboard (fault, transport, active keys, air duty, I2C health, IP, uptime) with "clear fault" and a remote STOP | `WebConfigPortal` |
| 34 | Stepper bellows could step past its software limit and wrap its position counter | Software end-stops at 0 and `stepperMaxSteps`; the open-loop limitation is stated as a validator warning | `StepperBellowsAirController`, `ConfigValidator` |
| 35 | No continuous integration | GitHub Actions: native tests (twice, second time `-Werror`) + a build matrix over all nine profiles | `.github/workflows/ci.yml` |

## Additional structural fixes

- Replaced `AIR_ANTICIPATION_MS` with a real non-blocking scheduler
  (`airPrechargeMs`, `keyPressDelayMs`, `keyReleaseDelayMs`, `airReleaseDelayMs`).
- Removed the misleading "16 notes / 30 servos / 32 array" inconsistency;
  standardised on 32 keys and documented the octave convention.
- Incomplete/announced-but-unimplemented features (audio auto-calibration, pitch
  bend / modulation stubs that only logged) were removed from the documentation
  or replaced with working equivalents, so the docs match the code.
- Logging now has `ERROR/WARN/INFO/DEBUG` levels selectable at compile time.
