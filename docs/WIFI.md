# WiFi / RTP-MIDI configuration

WiFi credentials are **never** stored in the source tree and never committed.
They live only in the ESP32's NVS (Preferences) and are loaded at runtime. The
`.gitignore` also blocks common secret filenames.

## Setting credentials without recompiling

Credentials are part of the persisted `BoardConfig` (`NetworkConfig`). Set them
at runtime through the serial console / provisioning path and they survive
reboots. Nothing needs to be recompiled to change network, transport, MIDI
channel, transposition or air parameters — all of it is in NVS.

If you prefer to seed defaults once at first boot, create a local, git-ignored
`secrets.h` (already listed in `.gitignore`) and reference it only from a local
build — do not commit it.

## Connection behaviour (non-blocking)

The RTP-MIDI transport uses `ConnectionManager`, a non-blocking state machine:

- `begin()` starts the WiFi association and returns immediately.
- The main loop keeps running safety checks and actuators while WiFi is down.
- Reconnection uses exponential backoff (`reconnectBaseMs` → `reconnectMaxMs`);
  it never spins or enters an infinite loop.
- On session loss the firmware runs `allNotesOff()` and a safe air stop.
- The current IP, session name and link state are available for a status display
  (`RtpMidiTransport::linkState()` / `wifiUp()`).

## Session

The AppleMIDI session name defaults to `NetworkConfig.sessionName` ("Servo
Melodica"). Connect from macOS "Audio MIDI Setup" → Network, or any RTP-MIDI
client (e.g. rtpMIDI on Windows).
