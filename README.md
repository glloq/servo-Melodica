# Servo Melodica — unified ESP32 firmware

A single, modular, testable firmware that drives a 32-key melodica with
servo-actuated keys (via PCA9685 boards) and a configurable air supply. One
source tree, many build profiles: each profile compiles one MIDI transport
around a **shared instrument core** — no duplicated business logic between the
BLE, WiFi, USB, DIN or serial builds. The **air system is chosen at runtime**
(web UI or serial console); WiFi builds expose a **web configuration page** and a
**setup hotspot** (long-press BOOT). The exact same tree builds in the **Arduino
IDE** (open `ServoMelodica/ServoMelodica.ino`) and in **PlatformIO**.

> Replaces the previous five parallel sketches (`Servo_melodica`,
> `Servo_melodica_Simple`, `Servo_melodica_ESP32_BLE`,
> `Servo_melodica_ESP32_WiFi`, `Calibration_Manual`). See
> [`MIGRATION.md`](MIGRATION.md).

## Contents

- [Architecture](#architecture)
- [Compatible boards](#compatible-boards)
- [Transports](#transports)
- [Air methods](#air-methods)
- [Wiring](#wiring)
- [Building — Arduino IDE](#building--arduino-ide)
- [Build profiles](#build-profiles-platformio)
- [Configuration (web UI & hotspot)](#configuration-web-ui--hotspot)
- [Calibration](#calibration)
- [WiFi configuration](#wifi-configuration)
- [Testing](#testing)
- [Troubleshooting](#troubleshooting)
- [Known limitations](#known-limitations)
- [Migration](#migration)

## Architecture

The transport-agnostic, actuator-agnostic core (`ServoMelodica/`) owns all instrument
logic: MIDI semantics, the note-state table, air-flow computation and the
non-blocking air/key scheduler. Everything hardware-specific sits behind three
interfaces — `IMidiTransport`, `IKeyDriver`, `IAirController` — so the same core
runs unchanged on every profile and on a PC under unit test.

```
transport (BLE/WiFi/DIN/USB/Serial) → MidiEvent → InstrumentController
                                                     ├── IKeyDriver     → Pca9685KeyDriver
                                                     └── IAirController → one of 9 air modules
SafetyManager supervises everything · ConfigStore (NVS) persists everything
```

Full details in [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md).

## Compatible boards

| Board | Profile family | Native USB-MIDI | Notes |
|-------|----------------|-----------------|-------|
| ESP32 DevKit / WROOM (`esp32dev`) | BLE, WiFi, DIN | No | Classic ESP32, BLE + WiFi |
| ESP32-S3 (`esp32-s3-devkitc-1`) | USB, BLE, WiFi | **Yes** | USB peripheral present |
| ESP32-S2 | USB | **Yes** | USB peripheral present, no BLE |
| Any host PC | `native_tests` | — | Runs the core under simulated drivers |

Native USB-MIDI is compiled **only** for S2/S3 (`-DMELODICA_USB_MIDI`); the
classic ESP32 has no device-mode USB and never builds that adapter.

## Transports

All behind `IMidiTransport`, one selected per profile:

| Transport | Build flag | Library |
|-----------|-----------|---------|
| BLE-MIDI | `TRANSPORT_BLE` | `lathoub/BLE-MIDI` (NimBLE-capable) |
| RTP-MIDI / AppleMIDI (WiFi) | `TRANSPORT_WIFI_RTP` | `lathoub/AppleMIDI` |
| DIN MIDI (UART 31250) | `TRANSPORT_DIN` | none (built-in `MidiParser`) |
| USB-MIDI (S2/S3) | `TRANSPORT_USB` | `Adafruit TinyUSB` |
| Serial-from-Pi | `TRANSPORT_SERIAL` | none |

Each transport supports a configurable filter: single channel, channel list,
omni, transposition, note-range limits and note→servo remapping
(`MidiFilterConfig`).

## Air methods

All behind `IAirController`, built on the shared `AirRamp` (threshold, min/max,
attack/release ramps, precharge, inversion). One selected per profile:

| Module | Build flag |
|--------|-----------|
| Proportional valve servo | `AIR_SERVO_VALVE` |
| On/off solenoid | `AIR_SOLENOID` |
| Stepped solenoids (flow stages) | `AIR_STEPPED_SOLENOID` |
| DC blower/fan (PWM) | `AIR_BLOWER_PWM` |
| DC pump + H-bridge (direction) | `AIR_PUMP_HBRIDGE` |
| Proportional valve (PWM/DAC) | `AIR_PWM_VALVE` |
| Stepper-driven bellows | `AIR_STEPPER_BELLOWS` |
| Permanent external air (digital enable) | `AIR_EXTERNAL` |
| Simulation (no actuator) | `AIR_SIMULATION` |

By default **every air module is compiled in and the active one is selected at
runtime** from the stored config (web UI or serial console) — a single binary
drives any air system. A PlatformIO profile that sets a `-DAIR_<X>` flag trims
the binary to that one module.

### Composite (multi-stage) air system

For real melodica builds the air path is usually more than one actuator. The
**Composite** air type builds the system from four independently configurable
stages, so e.g. *a solenoid main valve + a servo "servoflow" fed by a blower*
is just a configuration — no code change:

```
Source ──► Main valve ──► Flow ──► melodica keys (servos on up to 4 PCA9685)
  ▲
Pressure sensor (optional, regulates a reservoir source)
```

| Stage | Options |
|-------|---------|
| **Main valve** | Solenoid (on/off) · None (pass-through) |
| **Flow** ("servoflow") | Servo · PWM · DAC · None |
| **Source** | Blower/fan · Pump (H-bridge) · Reservoir (pressure-regulated) · External air |
| **Sensor** | Optional analog pressure sensor; a reservoir source is kept at a target pressure by bang-bang regulation |

Every stage, its pins, and the sensor calibration/target are set in the web UI.
The keys themselves are the up-to-64 servos across up to four PCA9685 boards
(`driverIndex = key/16`, `channel = key%16`).

### Air-flow policy

`AirPolicy` selects how active notes become flow: `Fixed`, `MaximumVelocity`,
`AverageVelocity`, `SumClamped`, `PolyphonyCompensated`. The demand folds in CC7
(volume) and CC11 (expression); **an effective demand of zero fully closes the
air**. Because the demand is recomputed on every note change, stopping the
loudest note of a chord makes the flow fall automatically.

## Wiring

Summary below; full table in [`docs/WIRING.md`](docs/WIRING.md).

| Signal | GPIO | Notes |
|--------|------|-------|
| I2C SDA / SCL | 21 / 22 | PCA9685 boards |
| PCA OE (shared) | 27 | active-low output-disable (**not** a power cut) |
| Air actuator | 13/14/27 | depends on selected module |
| DIN RX / TX | 16 / 17 | via opto-isolator, 31250 baud |
| Panic button | 0 | BOOT button, active-low |

Key→board/channel: `driverIndex = key/16`, `channel = key%16` (16 channels/board).

## Building — Arduino IDE

The whole firmware is a flat Arduino sketch, so no PlatformIO is required:

1. Install the **ESP32 boards** (Boards Manager → "esp32" by Espressif, 3.x).
2. Install these libraries via the Library Manager: *Adafruit PWM Servo Driver
   Library*, *ESP32Servo*, *MIDI Library* (FortySevenEffects), plus *AppleMIDI*
   (WiFi builds), *ESP32-BLE-MIDI* (BLE builds) or *Adafruit TinyUSB* (USB builds).
   `WebServer`, `DNSServer`, `WiFi`, `Wire`, `Preferences` ship with the core.
3. Open **`ServoMelodica/ServoMelodica.ino`**.
4. Choose your MIDI transport at the top of **`UserConfig.h`** (uncomment one
   `TRANSPORT_*`). The default is WiFi + RTP-MIDI, which also enables the web UI.
5. Select your board (e.g. "ESP32 Dev Module" or "ESP32S3 Dev Module") and Upload.

The air system does **not** need a compile-time choice — pick it later in the web
UI or serial console. All source files live flat in the sketch folder so the
Arduino IDE compiles them automatically.

## Build profiles (PlatformIO)

```ini
esp32_ble_servo_air         # BLE  + servo valve (lean, single air module)
esp32_ble_nimble_servo_air  # BLE  + servo valve, NimBLE stack (lower RAM)
esp32_wifi_servo_air        # WiFi + all air modules + web UI (default type: servo)
esp32_din_servo_air         # DIN  + servo valve (lean)
esp32_ble_pwm_blower        # BLE  + PWM blower (lean)
esp32_wifi_digital_valve    # WiFi + all air modules + web UI (default type: solenoid)
esp32_stepper_bellows       # BLE  + stepper bellows (lean)
esp32s3_usb_servo_air       # S3 native USB + servo valve (lean)
native_tests                # host unit tests
```

WiFi profiles compile every air module (runtime-selectable, web UI enabled);
`-DAIR_<X>` profiles are lean single-module binaries.

Exact build commands:

```sh
pio run -e esp32_ble_servo_air          # build BLE + servo
pio run -e esp32_wifi_servo_air         # build WiFi + servo
pio run -e esp32_din_servo_air          # build DIN + servo
pio run -e esp32_ble_pwm_blower         # build BLE + blower
pio run -e esp32_wifi_digital_valve     # build WiFi + solenoid
pio run -e esp32_stepper_bellows        # build BLE + stepper bellows
pio run -e esp32s3_usb_servo_air        # build S3 USB + servo

pio run -e esp32_ble_servo_air -t upload    # flash (append -t upload to any env)
pio run -e esp32_ble_servo_air -t monitor   # serial monitor
```

Dependency versions are pinned in `platformio.ini` for reproducible builds.

## Configuration (web UI & hotspot)

On **WiFi profiles**, everything is configurable from a browser — so any melodica
can be set up without editing code:

- **Melodica geometry**: number of keys (up to 64) and lowest MIDI note.
- **I2C / PCA9685**: board count (1–4), I2C addresses, SDA/SCL and OE pins.
- **Air system**: the type, all parameters and pins — including the full
  **composite** stack (main valve, servoflow, source, pressure sensor/reservoir).
- **MIDI**: channel filter, transposition, note range.
- **Per-key calibration** with live **Press/Release** test buttons that move the
  selected servo so you can dial in rest/pressed positions by eye.

Everything is saved to NVS.

**Reaching the page**

- If the device is joined to your network, browse to its IP (shown on the serial
  monitor and in the RTP session).
- If it is **not configured**, or you **long-press the BOOT button (≥ 3 s)**, it
  starts a WiFi hotspot **`ServoMelodica-Setup`** with a captive portal — connect
  to it and open <http://192.168.4.1/>.

A **short** BOOT press is a local **panic** (release all keys, close air).

Saving from the web page reboots the device to apply the new configuration
(needed when the air-system type changes). No recompilation is ever required.

On **non-WiFi profiles** (BLE/DIN/USB) the same settings are configured over the
serial console (`air <type>`, `pol <policy>`, calibration commands, then `s` and
`reboot`).

## Calibration

Calibration is **integrated** — no separate firmware. Open the serial monitor
(115200 baud) and use the console (type `?` for help):

```
k <n>    select key n           r <us>   set rest pulse
p <us>   set pressed pulse       lo <us>  set min limit
hi <us>  set max limit           v        toggle inversion
t        test selected key       ta       test all keys
d        reset key to default     s        save to NVS
x        export as C++            j        export as JSON
```

Values are applied live, saved to NVS (`s`) without recompiling, and can be
exported as C++ or JSON. Previous values are restored on reboot if you don't
save. Legacy angle-based calibration migrates via `migrateLegacyKey()`. On WiFi
builds the same per-key calibration is also editable from the web UI.

## WiFi configuration

Set credentials from the web UI (via the `ServoMelodica-Setup` hotspot on first
use). They live **only in NVS**, never in git. Reconnection is a non-blocking
state machine with exponential backoff; session loss triggers `allNotesOff()`
and a safe air stop. See [`docs/WIFI.md`](docs/WIFI.md).

## Testing

The core compiles and runs on a PC with simulated drivers — no ESP32 needed:

```sh
./scripts/run_native_tests.sh          # g++, prints "N checks, 0 failures"
# or, via PlatformIO:
pio run -e native_tests && .pio/build/native_tests/program
```

Covered cases include: PCA mapping for keys 0/15/16/31, out-of-range note
rejection, Note-On velocity 0, CC7 = 0, multi-velocity chord, stopping the
loudest note, sustain, All Notes Off, BLE/WiFi loss, WiFi reconnection, PCA I2C
fault, non-blocking precharge, per-servo mechanical limits, and legacy
calibration migration.

## Troubleshooting

| Symptom | Likely cause / fix |
|---------|-------------------|
| No keys move, log "I2C_FAULT" | Check PCA wiring/addresses; `SafetyManager` disabled outputs |
| One board's keys dead | That board failed I2C probe; check its address jumpers |
| Servos jitter when idle | Expected until idle-output-shedding drops OE; tune `idleDisableDelayMs` |
| Air never opens | CC7 or CC11 is 0, or demand below `startThreshold` |
| Air won't fully close | Check `airReleaseDelayMs`; CC7 = 0 must close immediately |
| WiFi never connects | Credentials not set in NVS (see `docs/WIFI.md`); firmware keeps retrying |
| Notes stuck after disconnect | Should not happen — loss triggers `allNotesOff`; check transport `connected()` |
| A servo strains at travel end | Tighten that key's `minPulseUs`/`maxPulseUs` in calibration |

Fault codes are consultable via `SafetyManager::faultText()`.

## Known limitations

- **True air anticipation** needs either added latency or the host sending
  events early; the firmware provides scheduled precharge, not prediction.
- Pitch bend and polyphonic aftertouch are accepted but not mechanically
  realised (a melodica cannot bend a single key's pitch).
- The stepper-bellows module uses simple constant-rate stepping (no acceleration
  profile); adequate for slow bellows, not high-speed positioning.
- Native USB-MIDI requires an S2/S3; the classic ESP32 cannot provide it.

## Migration

See [`MIGRATION.md`](MIGRATION.md) for the per-file migration table and
[`docs/DEFECTS_FIXED.md`](docs/DEFECTS_FIXED.md) for the corrected defects. The
32-key count and the **C4 = MIDI 60** octave convention (lowest key F4 = 65,
highest C7 = 96) are documented there.
