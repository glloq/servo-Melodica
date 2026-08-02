#ifndef USERCONFIG_H
#define USERCONFIG_H

// ===========================================================================
//  Compile-time selection for Arduino IDE users.
//
//  In the Arduino IDE there are no per-sketch build flags, so choose the MIDI
//  transport here by leaving exactly ONE of the TRANSPORT_* defines below
//  uncommented. PlatformIO passes these as -D flags instead and its choice
//  overrides this file (the block is skipped when a flag is already defined).
//
//  The AIR SYSTEM is NOT selected here — every air module is compiled in by
//  default and the active one is chosen at runtime from the web UI / serial
//  console (BoardConfig::air.type). Add -DAIR_<X> in PlatformIO only if you want
//  a lean single-module binary.
// ===========================================================================

#if !defined(TRANSPORT_BLE) && !defined(TRANSPORT_WIFI_RTP) &&                  \
    !defined(TRANSPORT_DIN) && !defined(TRANSPORT_USB) && !defined(TRANSPORT_SERIAL)
// ---- Arduino IDE: pick ONE transport ----
#define TRANSPORT_WIFI_RTP  // default: WiFi + RTP-MIDI (enables the web config UI)
// #define TRANSPORT_BLE
// #define TRANSPORT_DIN
// #define TRANSPORT_USB
// #define TRANSPORT_SERIAL
#endif

// The web configuration UI + captive hotspot needs WiFi. Enable it automatically
// on WiFi builds unless explicitly disabled with -DMELODICA_NO_WEB_CONFIG.
#if defined(TRANSPORT_WIFI_RTP) && !defined(MELODICA_WEB_CONFIG) &&             \
    !defined(MELODICA_NO_WEB_CONFIG)
#define MELODICA_WEB_CONFIG
#endif

#endif  // USERCONFIG_H
