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
  - `ConnectionManager` — non-blocking reconnection with exponential backoff.
  - `SafetyManager` — watchdogs + safe-state enforcement.
  - `Config`, `Defaults`, `Calibration`, `KeyPulse`, `UserConfig` — config + maths.
- **Key driver** — `Pca9685KeyDriver` (the reference `IKeyDriver`).
- **Air modules** — nine `IAirController` modules on a shared `AirRamp`, plus
  `AirControllerFactory` which placement-news the runtime-selected module.
- **Transports** — five `IMidiTransport` adapters (BLE, RTP, DIN, USB, serial).
- **Platform** — ESP32 NVS store (`ConfigStore`), serial `CalibrationConsole`,
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

## Data flow per loop iteration (non-blocking)

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
