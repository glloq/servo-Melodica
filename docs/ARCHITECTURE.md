# Architecture

```
                         MIDI wire (BLE / WiFi / DIN / USB / Serial)
                                        │
                          ┌─────────────┴──────────────┐
                          │        IMidiTransport        │   adapters in lib/transports
                          │  BLE · RTP · DIN · USB · Ser │   (decode → MidiEvent queue)
                          └─────────────┬──────────────┘
                                        │  MidiEvent (common struct)
                                        ▼
   ┌───────────────────────────────────────────────────────────────────────┐
   │                        InstrumentController  (lib/core)                 │
   │  MidiFilter → NoteState table → air demand (AirPolicy) → schedulers     │
   │  transport-agnostic · actuator-agnostic · no BLE/WiFi/USB includes      │
   └───────────────┬───────────────────────────────────┬───────────────────┘
                   │ IKeyDriver                          │ IAirController
                   ▼                                     ▼
        ┌────────────────────┐                ┌────────────────────────────┐
        │ Pca9685KeyDriver   │                │ one of 9 air modules        │
        │ (lib/drivers_key)  │                │ (lib/drivers_air) on AirRamp │
        └────────────────────┘                └────────────────────────────┘

        SafetyManager supervises the core, key driver and air module and forces a
        safe state on any critical fault. ConfigStore (NVS) persists everything.
```

## Layers

- **`lib/core/`** — platform-independent, host-compilable. No Arduino, BLE, WiFi,
  USB or vendor headers. This is the only place the instrument logic lives.
  - `MidiEvent`, `IMidiTransport`, `IKeyDriver`, `IAirController` — the contracts.
  - `InstrumentController` — note engine + air-demand + non-blocking scheduler.
  - `AirPolicy`, `AirRamp` — flow computation and actuator shaping.
  - `MidiFilter`, `MidiParser`, `EventQueue` — MIDI plumbing.
  - `ConnectionManager` — non-blocking reconnection with exponential backoff.
  - `SafetyManager` — watchdogs + safe-state enforcement.
  - `Config`, `Defaults`, `Calibration`, `KeyPulse` — configuration + maths.
- **`lib/drivers_key/`** — `Pca9685KeyDriver` (the reference `IKeyDriver`).
- **`lib/drivers_air/`** — nine `IAirController` modules on a shared `AirRamp`.
- **`lib/transports/`** — five `IMidiTransport` adapters.
- **`lib/platform/`** — ESP32-specific NVS store and serial calibration console.
- **`src/main.cpp`** — the composition root: picks one transport and one air
  module by build flag and wires everything together with static objects.
- **`test/`** — host unit tests using simulated drivers.

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
