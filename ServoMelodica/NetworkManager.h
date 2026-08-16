#ifndef PLATFORM_NETWORKMANAGER_H
#define PLATFORM_NETWORKMANAGER_H

#if defined(ARDUINO)

#include <Arduino.h>
#include <WiFi.h>

#include "Log.h"

namespace melodica {

// Single owner of the ESP32 WiFi mode.
//
// Before this existed, two components fought over the same radio: the config
// portal set WIFI_AP_STA to keep the setup hotspot alive, and the RTP-MIDI
// transport's link driver called WiFi.mode(WIFI_STA) on every reconnect attempt
// — which, with the exponential backoff retrying in the background, tore the
// hotspot down under the operator's browser mid-configuration.
//
// Now the transport only ever asks for "connect the station", the portal only
// ever asks for "bring the access point up", and this class computes the single
// mode that satisfies both (STA / AP / AP_STA). Nothing else in the firmware
// calls WiFi.mode().
class NetworkManager {
public:
    static NetworkManager& instance() {
        static NetworkManager mgr;
        return mgr;
    }

    // Idempotent bring-up. Keeps credentials out of the radio's own NVS copy so
    // the stored BoardConfig stays the only source of truth.
    void begin() {
        if (begun_) return;
        begun_ = true;
        WiFi.persistent(false);
        WiFi.setAutoReconnect(false);  // ConnectionManager owns the retry policy
        applyMode();
    }

    // Start (or restart) a station connection, preserving any active AP.
    void connectStation(const char* ssid, const char* pass) {
        begin();
        staWanted_ = true;
        applyMode();
        if (ssid && ssid[0] != '\0') {
            WiFi.begin(ssid, pass);
        }
    }

    // Bring the configuration hotspot up without disturbing the station.
    bool startAccessPoint(const char* apSsid, const char* apPass = nullptr) {
        begin();
        if (apActive_) return true;
        apWanted_ = true;
        applyMode();
        bool ok = (apPass && apPass[0] != '\0') ? WiFi.softAP(apSsid, apPass)
                                                : WiFi.softAP(apSsid);
        apActive_ = ok;
        if (!ok) {
            apWanted_ = false;
            applyMode();
            LOG_ERROR("Could not start hotspot '%s'", apSsid);
        }
        return ok;
    }

    void stopAccessPoint() {
        if (!apActive_) return;
        WiFi.softAPdisconnect(true);
        apActive_ = false;
        apWanted_ = false;
        applyMode();
    }

    bool apActive() const { return apActive_; }
    bool stationUp() const { return WiFi.status() == WL_CONNECTED; }
    IPAddress stationIp() const { return WiFi.localIP(); }
    IPAddress apIp() const { return WiFi.softAPIP(); }

private:
    NetworkManager() = default;

    // The one place a WiFi mode is decided.
    void applyMode() {
        wifi_mode_t want;
        if (apWanted_ && staWanted_) want = WIFI_AP_STA;
        else if (apWanted_) want = WIFI_AP;
        else want = WIFI_STA;
        if (want != mode_) {
            mode_ = want;
            WiFi.mode(want);
        }
    }

    bool begun_ = false;
    bool staWanted_ = false;
    bool apWanted_ = false;
    bool apActive_ = false;
    wifi_mode_t mode_ = WIFI_OFF;
};

}  // namespace melodica

#endif  // ARDUINO
#endif  // PLATFORM_NETWORKMANAGER_H
