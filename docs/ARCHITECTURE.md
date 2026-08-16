# Architecture

```
        MIDI wire (BLE / WiFi / DIN / USB / Serial)
                        │
                        ▼
   IMidiTransport adapters  ──decode──►  MidiEvent (common struct)
                        │
                        ▼
   ┌───────────────────────────────────────────────────────────────┐
   │                     InstrumentController                       │
   │  MidiFilter → NoteState table → air demand (AirPolicy) → sched │
   │  transport-agnostic · actuator-agnostic · no BLE/WiFi includes │
   └───────────────┬─────────────────────────────┬─────────────────┘
                   │ IKeyDriver                    │ IAirController
                   ▼                               ▼
        ┌────────────────────┐        ┌────────────────────────────┐
        │  Pca9685KeyDriver  │        │  one of 9 air modules       │
        │                    │        │  (via AirControllerFactory) │
        └────────────────────┘        └────────────────────────────┘

   SafetyManager supervises the core, key driver and air module and forces a
   safe state on any critical fault. ConfigStore (NVS) persists everything;
   WebConfigPortal (WiFi builds) exposes it all over a browser + setup hotspot.
```

## Layers

All firmware files live flat in `ServoMelodica/` (an Arduino sketch folder), so
the same tree builds in the Arduino IDE and PlatformIO. Files are grouped here by
role, not by directory:

- **Core** — platform-independent, host-compilable. No Arduino, BLE, WiFi, USB or
  vendor headers. This is the only place the instrument logic lives.
  - `MidiEvent`, `IMidiTransport`, `IKeyDriver`, `IAirController` — the contracts.
  - `InstrumentController` — note engine + air-demand + non-blocking scheduler.
  - `AirPolicy`, `AirRamp` — flow computation and actuator shaping.
  - `MidiFilter`, `MidiParser`, `EventQueue` — MIDI plumbing.
  - `MidiLinkWatchdog` — link state for byte-stream transports (DIN / serial):
    an open UART is connected; supervision is armed by Active Sensing only.
  - `ConnectionManager` — non-blocking reconnection with exponential backoff.
  - `SafetyManager` — watchdogs + safe-state enforcement.
  - `AirRunTracker` — uninterrupted actuator run time, feeding the pump watchdog.
  - `PanicInput` — debounce + edge detection for the emergency-stop input.
  - `HardwareCaps` — what each GPIO can do, per ESP32 variant.
  - `ConfigValidator` — refuses configurations the hardware cannot honour.
  - `Config`, `Defaults`, `Calibration`, `ConfigLegacy`, `KeyPulse`, `UserConfig`
    — config, persisted-record layouts + migrations, and maths.
- **Key driver** — `Pca9685KeyDriver` (the reference `IKeyDriver`).
- **Air modules** — nine `IAirController` modules on a shared `AirRamp`, plus
  `AirControllerFactory` which placement-news the runtime-selected module.
- **Transports** — five `IMidiTransport` adapters (BLE, RTP, DIN, USB, serial).
- **Platform** — ESP32 NVS store (`ConfigStore`), serial `CalibrationConsole`,
  `NetworkManager` (the single owner of the WiFi mode: the RTP transport asks for
  a station, the portal asks for an access point, this decides STA/AP/AP_STA),
  and `WebConfigPortal` (web UI + captive hotspot, WiFi builds only).
- **`ServoMelodica.ino`** — the composition root: picks one transport by build
  flag / `UserConfig.h`, builds the runtime-selected air module, and wires
  everything with static objects (no heap).
- **`test/`** — host unit tests using simulated drivers.

## Air-module selection (compile-time vs runtime)

Every air module is compiled in by default; `AirControllerFactory` constructs the
one named by `BoardConfig::air.type` at `setup()` (placement-new into static
storage — no heap, not in the real-time path). The web UI / serial console change
that type in NVS and reboot to apply. A `-DAIR_<X>` build compiles only module X
for a lean binary.

The `Composite` type builds a 4-stage system (source → main valve → flow →
optional sensor) from small interfaces in `AirStages.h`: `IMainValve`,
`IFlowActuator`, `IAirSource`, `IPressureSensor`. `CompositeAirController` is pure
orchestration logic (host-tested with mocks); the ESP32 actuators live in
`AirStages_ESP32.h`, and `ReservoirRegulator` is a pure bang-bang controller for
a pressure-regulated tank. Geometry (`InstrumentConfig.noteCount`,
`firstMidiNote`) is runtime too, with arrays sized to the `MAX_NOTES = 64` cap.

## Supervision contracts

Two things the core asks its hardware about, and what it does with the answers:

- **`IKeyDriver::healthy()`** — `Pca9685KeyDriver` re-probes one board per
  `healthCheckIntervalMs` (round-robin), so a board that dies *after* boot is
  detected within `boardCount × interval`; a board that comes back is
  re-initialised and its keys restored.
- **`IAirController::isRunning()` / `runtimeMs()` / `healthy()` / `fault()`** —
  every module tracks its own uninterrupted run time (`AirRunTracker`, driven by
  the pre-inversion ramp level, plus the source's own tracker for a reservoir
  that fills with no notes playing). `SafetyManager` enforces
  `maxPumpRunMs` (→ `PumpOverrun`) and latches `AirNotInitialised` for a module
  that refuses to run — unusable pin, failed bring-up, or a regulated reservoir
  with no valid pressure sensor. A faulted module is never commanded.

`SafetyManager` keeps the FIRST cause of a shutdown: a second symptom does not
overwrite it, and the safe stop is not re-run on every loop iteration.

## Data flow per loop iteration (non-blocking)

0. The emergency-stop input is read first, so nothing else can delay it.
1. `transport.update()` decodes wire bytes into a `MidiEvent` queue.
2. The core drains the queue → `InstrumentController::handleEvent()`.
3. `handleEvent` updates the `NoteState` table and recomputes the `AirDemand`.
4. `InstrumentController::update(nowUs)` advances the key/air schedulers and
   calls `keyDriver.update()` + `airController.update()`.
5. `SafetyManager::update()` runs the watchdogs.

Nothing in this path blocks; network connection and reconnection happen inside
`ConnectionManager` state transitions, one step per iteration.

## Multi-channel note policy

A physical key is **reference-counted across MIDI channels**: it stays pressed
while any channel holds a Note On for its note and releases only when the last
holding channel sends Note Off (or sustain is released). Stored velocity is
last-wins and feeds the air demand. See `test/test_instrument.cpp`
(`run_instrument_multichannel`).

## Air/key synchronisation

Real anticipation is impossible without either added latency or the host sending
events early. The firmware provides a genuine non-blocking scheduler instead of
the old blocking `AIR_ANTICIPATION_MS`:

- `airPrechargeMs` — open air this long before pressing the key.
- `keyPressDelayMs` / `keyReleaseDelayMs` — offset key motion.
- `airReleaseDelayMs` — keep air flowing briefly after the last note.

These delays are honoured with `micros()` deadlines, never `delay()`.

## Configuration: validation and persistence

`ConfigValidator` answers "can this board actually do what this configuration
asks?" using `HardwareCaps` (per-variant GPIO capabilities: existence,
input-only pads, SPI-flash pins, ADC/DAC channels, strapping pins, LEDC channel
count). It returns errors *and* warnings, so the UI can refuse an impossible
setup (64 keys on one PCA9685, a solenoid on GPIO34, two jobs on one pin, a
regulated reservoir with no sensor) while merely flagging a risky one (a
strapping pin, a disabled watchdog, an ADC2 sensor on a WiFi build). It is pure
logic and fully host-tested; the web portal, the serial console (`val`) and the
boot sequence all use the same code.

Persistence is a header-tagged record:

```
ConfigRecordHeader { magic 'MELO', schema, size, crc32 }  +  BoardConfig
```

The header is what makes migration possible: a stored record is identified and
length-checked before anything interprets its bytes. Older layouts are frozen in
`ConfigLegacy.h` with a field-by-field converter (`legacy::migrateV2`), so a
schema change that resizes `BoardConfig` upgrades a user's calibration instead of
silently discarding it — which is exactly what the previous "size must match
exactly, then consider migrating" ordering did.
