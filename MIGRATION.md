# Migration guide — from the old sketches to the unified firmware

The repository previously held **five parallel copies** of the firmware, four of
which duplicated the same instrument logic. They are replaced by a single
modular source tree (`src/` + `lib/`) with per-transport / per-air build
profiles. This document maps every old file to its fate and lists the defects
that were corrected.

## Migration table

| Old file | Fate | Replacement / notes |
|----------|------|---------------------|
| **Servo_melodica/Servo_melodica.ino** | Replaced | `src/main.cpp` composition root; no more `new Instrument()` |
| **Servo_melodica/settings.h** | Replaced | `lib/core/Config.h` + `lib/core/Defaults.h` + NVS (`ConfigStore`) |
| **Servo_melodica/instrument.h / .cpp** | Merged | `lib/core/InstrumentController.{h,cpp}` (single shared core) |
| **Servo_melodica/ServoController.h / .cpp** | Replaced | `lib/drivers_key/Pca9685KeyDriver.h` + `lib/core/KeyPulse.h` |
| **Servo_melodica/MidiHandler.h / .cpp** | Replaced | `lib/core/MidiParser.h` + transport adapters + core event handling |
| **Servo_melodica/AudioCalibration.h / .cpp** | Obsolete | Incomplete audio auto-calibration removed; superseded by the serial `CalibrationConsole` |
| **Servo_melodica_Simple/** (all files) | Obsolete | Feature-subset of the main version; folded into the unified core |
| **Servo_melodica_ESP32_BLE/instrument.\*, ServoController.\*** | Removed (duplicate) | Byte-for-byte copy of the core; now the shared `lib/core` |
| **Servo_melodica_ESP32_BLE/\*.ino** | Replaced | `BleMidiTransport` behind `IMidiTransport` + `src/main.cpp` |
| **Servo_melodica_ESP32_BLE/settings.h** | Replaced | `Config.h` / NVS |
| **Servo_melodica_ESP32_WiFi/instrument.\*, ServoController.\*** | Removed (duplicate) | Same shared core |
| **Servo_melodica_ESP32_WiFi/\*.ino** | Replaced | `RtpMidiTransport` + `WifiLink` + non-blocking `ConnectionManager` |
| **Servo_melodica_ESP32_WiFi/settings.h** | Replaced | `Config.h` / NVS — **committed WiFi credentials removed** |
| **Calibration_Manual/Calibration_Manual.ino** | Merged | Integrated serial `CalibrationConsole` (no separate firmware) |
| **Calibration_Manual/README.md** | Merged | Folded into `docs/CALIBRATION` section of the README |

No old file keeps a private copy of the instrument core — that requirement of the
brief is now structurally enforced: there is exactly one `InstrumentController`.

## Migrating your calibration data

The old firmware stored per-servo `initialAngles[]` (degrees) and `sensRot[]`
(+1/-1). The new firmware stores absolute pulse widths per key
(`KeyCalibration`). `lib/core/Calibration.h` provides `migrateLegacyKey()` which
converts an old `(angle, direction)` pair plus the old `ANGLE_NOTE_ON` travel
into the new rest/pressed pulses. See the "migration d'une ancienne calibration"
unit test in `test/test_keydriver.cpp`.

## Key/note count and octave convention

The old code was inconsistent (titles said "16 notes", comments said "30
servos", arrays were sized 32). The unified firmware standardises on **32 keys**.

- `NUMBER_OF_NOTES = 32`, `FIRST_MIDI_NOTE = 65`.
- Octave naming uses the **C4 = MIDI 60** convention (middle C = C4). Therefore
  the lowest key is **F4 (65)** and the highest is **C7 (96)**.
- If your host uses the C3 = 60 convention, shift octave labels down by one (the
  MIDI numbers are unchanged); use the transpose setting if your controller
  spans a different range.
