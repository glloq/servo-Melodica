# WiFi / RTP-MIDI configuration

WiFi credentials are **never** stored in the source tree and never committed.
They live only in the ESP32's NVS (Preferences) and are loaded at runtime. The
`.gitignore` also blocks common secret filenames.

## Provisioning without recompiling — the setup hotspot

On first boot with no stored credentials (or after a long BOOT press), the device
starts a WiFi hotspot **`ServoMelodica-Setup`** with a captive portal. Connect to
it, open <http://192.168.4.1/>, enter your SSID/password (and anything else) and
save — the device stores them in NVS and reboots onto your network.

Once joined, the same web page is reachable at the device's network IP, so you can
change network, MIDI channel, transposition, air system and parameters later
without touching the hotspot and without recompiling — all of it lives in NVS.

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
