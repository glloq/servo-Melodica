#ifndef TRANSPORTS_RTPMIDITRANSPORT_H
#define TRANSPORTS_RTPMIDITRANSPORT_H

#if defined(ARDUINO)

#include <Arduino.h>
#include <WiFi.h>

#include "ConnectionManager.h"
#include "EventQueue.h"
#include "IMidiTransport.h"
#include "Log.h"
#include "NetworkManager.h"

namespace melodica {

// WiFi link driver for the reconnection state machine. Wrapping WiFi behind
// ILink keeps ConnectionManager (and its exponential backoff) transport-neutral
// and unit-testable with a mock.
//
// It asks NetworkManager to connect the station and never touches WiFi.mode()
// itself, so a retry storm can no longer knock down the configuration hotspot.
class WifiLink : public ILink {
public:
    WifiLink(const char* ssid, const char* pass) : ssid_(ssid), pass_(pass) {}
    void beginConnect() override {
        NetworkManager::instance().connectStation(ssid_, pass_);
    }
    bool isUp() const override { return NetworkManager::instance().stationUp(); }

private:
    const char* ssid_;
    const char* pass_;
};

// The AppleMIDI session name is fixed by the APPLEMIDI_CREATE_INSTANCE macro at
// compile time, but the operator can edit network.sessionName from the web UI.
// Apply it at runtime when the library version exposes a setter, and say so in
// the log when it does not, instead of leaving a control that quietly does
// nothing.
//
// Applied before the session is advertised, so invitations carry the chosen
// name. (AppleMIDI keeps the name unless the sketch defines NO_SESSION_NAME to
// save 100 bytes — with that define the library's own setter is a no-op and the
// instrument keeps its built-in name.)
template <typename T>
inline auto applySessionName(T& session, const char* name, int)
    -> decltype(session.setName(name), void()) {
    session.setName(name);
    LOG_INFO("RTP-MIDI session name: %s", name);
}
template <typename T>
inline void applySessionName(T&, const char* name, long) {
    LOG_WARN("AppleMIDI library has no setName(): session stays as built, not '%s'", name);
}

// RTP-MIDI (AppleMIDI over WiFi) adapter. Templated on the MIDI interface and
// AppleMIDI session types to match the library's macro-generated instances.
//
// Non-blocking by construction: begin() only kicks off the WiFi state machine
// and returns. update() advances the ConnectionManager (which retries with
// growing backoff and never spins), starts the AppleMIDI session once WiFi is
// up, and drains MIDI. The main loop keeps running safety and actuators while
// the link is down; connected() drives the safe-stop on session loss.
template <typename MidiType, typename AppleMidiType>
class RtpMidiTransport : public IMidiTransport {
public:
    RtpMidiTransport(MidiType& midi, AppleMidiType& apple, const char* ssid,
                     const char* pass, uint16_t backoffBaseMs, uint16_t backoffMaxMs,
                     const char* sessionName = nullptr)
        : midi_(midi),
          apple_(apple),
          link_(ssid, pass),
          conn_(link_, backoffBaseMs, backoffMaxMs),
          sessionName_(sessionName) {
        self_ = this;
    }

    bool begin() override {
        conn_.begin(millis());
        return true;  // non-blocking: link comes up later
    }

    void update() override {
        conn_.update(millis());
        if (conn_.justConnected() && !sessionStarted_) {
            // Bring the AppleMIDI session up exactly once WiFi is available.
            if (sessionName_ && sessionName_[0] != '\0') {
                applySessionName(apple_, sessionName_, 0);
            }
            midi_.begin(MIDI_CHANNEL_OMNI);
            apple_.setHandleConnected(
                [](const auto&, const char*) { if (self_) self_->sessionConnected_ = true; });
            apple_.setHandleDisconnected(
                [](const auto&) { if (self_) self_->sessionConnected_ = false; });
            midi_.setHandleNoteOn(&RtpMidiTransport::onNoteOn);
            midi_.setHandleNoteOff(&RtpMidiTransport::onNoteOff);
            midi_.setHandleControlChange(&RtpMidiTransport::onControlChange);
            midi_.setHandlePitchBend(&RtpMidiTransport::onPitchBend);
            midi_.setHandleProgramChange(&RtpMidiTransport::onProgramChange);
            sessionStarted_ = true;
        }
        if (conn_.justLost()) {
            sessionConnected_ = false;  // WiFi dropped: session is gone
        }
        if (conn_.connected()) {
            midi_.read();
        }
    }

    bool read(MidiEvent& event) override { return queue_.pop(event); }

    // "Connected" for the instrument means an active RTP session over live WiFi.
    bool connected() const override { return conn_.connected() && sessionConnected_; }

    const char* name() const override { return "RTP-MIDI"; }

    // Exposed for the status display (IP / link state).
    LinkState linkState() const { return conn_.state(); }
    bool wifiUp() const { return conn_.connected(); }

private:
    static void push(const MidiEvent& e) { if (self_) self_->queue_.push(e); }
    static void onNoteOn(byte ch, byte n, byte v) {
        MidiEvent e; e.type = MidiEventType::NoteOn; e.channel = ch - 1;
        e.data1 = n; e.data2 = v; e.timestampUs = micros(); push(e);
    }
    static void onNoteOff(byte ch, byte n, byte v) {
        MidiEvent e; e.type = MidiEventType::NoteOff; e.channel = ch - 1;
        e.data1 = n; e.data2 = v; e.timestampUs = micros(); push(e);
    }
    static void onControlChange(byte ch, byte num, byte val) {
        MidiEvent e; e.type = MidiEventType::ControlChange; e.channel = ch - 1;
        e.data1 = num; e.data2 = val; e.timestampUs = micros(); push(e);
    }
    static void onPitchBend(byte ch, int bend) {
        MidiEvent e; e.type = MidiEventType::PitchBend; e.channel = ch - 1;
        e.signedValue = static_cast<int16_t>(bend); e.timestampUs = micros(); push(e);
    }
    static void onProgramChange(byte ch, byte program) {
        MidiEvent e; e.type = MidiEventType::ProgramChange; e.channel = ch - 1;
        e.data1 = program; e.timestampUs = micros(); push(e);
    }

    MidiType& midi_;
    AppleMidiType& apple_;
    WifiLink link_;
    ConnectionManager conn_;
    EventQueue<64> queue_;
    const char* sessionName_ = nullptr;
    bool sessionStarted_ = false;
    volatile bool sessionConnected_ = false;
    static inline RtpMidiTransport* self_ = nullptr;
};

}  // namespace melodica

#endif  // ARDUINO
#endif  // TRANSPORTS_RTPMIDITRANSPORT_H
