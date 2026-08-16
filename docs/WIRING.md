# Wiring

Default pin assignments for an ESP32 DevKit (`esp32dev`). Air-actuator pins and
I2C are part of the persisted config (`AirModuleWiring` / `I2cConfig`) and are
editable from the web UI (WiFi builds) or the serial console — no recompilation
needed. The compile-time defaults live in `Config.h`.

## I2C bus (PCA9685 key boards)

| Signal | ESP32 GPIO | Notes |
|--------|-----------|-------|
| SDA | 21 | `I2cConfig.sdaPin` |
| SCL | 22 | `I2cConfig.sclPin` |
| OE (shared) | e.g. 27 | Active-low **output disable**, see below |
| V+ | external 5–6 V | Servo power — **not** from the ESP32 |
| VCC | 3.3 V | Logic supply |
| GND | GND | Common ground with servo supply |

Board addresses are set by the A0–A5 solder jumpers: `0x40`, `0x41`, `0x42`, …
Configure them in `KeyDriverConfig.addresses[]` and set `boardCount`.

### Key-to-channel mapping

```
driverIndex = keyIndex / 16      channel = keyIndex % 16
key  0 → board 0 ch 0     key 15 → board 0 ch 15
key 16 → board 1 ch 0     key 31 → board 1 ch 15
```

### About the OE pin

`OE` (Output Enable) is **active-low** and only tri-states the PWM outputs — it
does **not** cut power to the servos. Driving `OE` LOW enables the outputs; HIGH
floats them (the firmware uses this to stop commanding the servos when idle, to
reduce chatter/noise). To actually remove servo power you must switch `V+`
externally (e.g. a MOSFET/relay), which is independent of `OE`.

## Air actuator pins (per module)

Set in `AirModuleWiring` (config / web UI). Only the pins for the selected module
are used.

| Module (build flag) | Pins used |
|---------------------|-----------|
| `AIR_SERVO_VALVE` | `primaryPin` (servo PWM) |
| `AIR_SOLENOID` | `primaryPin` (digital) |
| `AIR_STEPPED_SOLENOID` | `stagePins[0..n]` (digital) |
| `AIR_BLOWER_PWM` | `primaryPin` (LEDC PWM), optional `enablePin` |
| `AIR_PUMP_HBRIDGE` | `primaryPin` (PWM), `secondaryPin` (direction) |
| `AIR_PWM_VALVE` | `primaryPin` (LEDC PWM or DAC GPIO25/26) |
| `AIR_STEPPER_BELLOWS` | `stepPin`, `dirPin` |
| `AIR_EXTERNAL` | `enablePin` (digital permission) |
| `AIR_SIMULATION` | none |

Pins are checked against the chip before use (`HardwareCaps` / `ConfigValidator`):
an input-only pad (GPIO34–39 on the classic ESP32), an SPI-flash pin (GPIO6–11) or
a DAC request on a pin without a DAC is refused, logged and left undriven rather
than half-driven.

## Transport-specific pins

| Transport | Wiring |
|-----------|--------|
| BLE / WiFi | none (radio) |
| DIN | UART2 RX=GPIO16, TX=GPIO17 via a 6N138/H11L1 opto @ 31250 baud |
| USB (S2/S3) | native USB connector |
| Serial | USB-serial / UART to the host (e.g. Raspberry Pi) |

### Link supervision on DIN and serial

A byte-stream MIDI link carries no idle traffic, so **silence is not a
disconnection** — a five-second held note sends nothing between its Note On and
Note Off. The firmware therefore treats an open UART as connected.

If you *want* the instrument to stop when the sender disappears, have the sender
emit **Active Sensing (`0xFE`)** at least every 300 ms (any standard MIDI source
option, or one extra byte in a Raspberry Pi bridge loop). That arms supervision:
from the first `0xFE`, silence longer than `safety.activeSensingTimeoutMs`
(default 300 ms) counts as a lost link and releases the keys.

## Buttons

| Input | Role |
|-------|------|
| `GPIO0` (BOOT, active-low) | **Configuration**: short press = local panic, long press (≥ 3 s) = start the WiFi setup hotspot + web UI |
| `safety.panicPin` (default: none) | **Emergency stop**: fires on the transition into the active state, while the button is still held |

### Emergency stop (recommended)

The BOOT button is a configuration control: it only acts when you *release* a
short press, and a press between 1.5 s and 3 s does nothing. Wire a real stop
instead — set `safety.panicPin` in the web UI (Safety section):

```
                 3.3V (internal pull-up)
                        │
   GPIO<panicPin> ──────┴───────[ NC mushroom button ]────── GND
```

Use a **normally-closed** button and tick *"Stop when pin reads HIGH"*
(`panicActiveHigh = true`). The loop is closed in normal operation, so pressing
the button **or cutting the wire** both read HIGH and stop the instrument
(release all keys, close the air, disable the servo outputs, latch a `PANIC`
fault). Clear the fault from the web page or the serial console once the cause is
dealt with.
