# Wiring

Default pin assignments for an ESP32 DevKit (`esp32dev`). Override in
`src/main.cpp` (`makeAirWiring()` and the pin constants) or via `Config` for I2C.

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

`makeAirWiring()` in `src/main.cpp`. Only the pins for the selected module are used.

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

## Transport-specific pins

| Transport | Wiring |
|-----------|--------|
| BLE / WiFi | none (radio) |
| DIN | UART2 RX=GPIO16, TX=GPIO17 via a 6N138/H11L1 opto @ 31250 baud |
| USB (S2/S3) | native USB connector |
| Serial | USB-serial / UART to the host (e.g. Raspberry Pi) |

## Panic button

`GPIO0` (BOOT button) is read active-low as a local panic input.
