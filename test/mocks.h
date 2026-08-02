#ifndef TEST_MOCKS_H
#define TEST_MOCKS_H

#include <cstdint>
#include <vector>

#include "IAirController.h"
#include "IKeyDriver.h"
#include "IMidiTransport.h"
#include "PcaMapping.h"

// Simulated hardware for host-side testing of the instrument core.
namespace melodica {

class MockKeyDriver : public IKeyDriver {
public:
    explicit MockKeyDriver(uint16_t count) : pressed_(count, false), count_(count) {}

    bool begin() override { begun_ = true; return healthy_; }
    uint16_t keyCount() const override { return count_; }
    void pressKey(uint16_t i) override {
        if (i < count_) { pressed_[i] = true; ++pressEvents_; lastLocation_ = pcaLocationForServo(i); }
    }
    void releaseKey(uint16_t i) override { if (i < count_) pressed_[i] = false; }
    void releaseAll() override { for (auto&& p : pressed_) p = false; ++releaseAllCalls_; }
    void update(uint32_t) override {}
    void setOutputsEnabled(bool e) override { outputsEnabled_ = e; }
    bool healthy() const override { return healthy_; }

    // Test hooks.
    bool isPressed(uint16_t i) const { return i < count_ && pressed_[i]; }
    int pressedCount() const { int n = 0; for (bool p : pressed_) if (p) ++n; return n; }
    void setHealthy(bool h) { healthy_ = h; }
    bool outputsEnabled() const { return outputsEnabled_; }
    int releaseAllCalls() const { return releaseAllCalls_; }
    PcaLocation lastLocation() const { return lastLocation_; }

private:
    std::vector<bool> pressed_;
    uint16_t count_;
    bool begun_ = false;
    bool healthy_ = true;
    bool outputsEnabled_ = true;
    int pressEvents_ = 0;
    int releaseAllCalls_ = 0;
    PcaLocation lastLocation_{0, 0};
};

class MockAirController : public IAirController {
public:
    bool begin() override { return true; }
    void setDemand(const AirDemand& d) override { demand_ = d; ++setCalls_; }
    void update(uint32_t) override {}
    void stop() override { stopped_ = true; }
    void emergencyStop() override { emergency_ = true; demand_ = AirDemand{}; }

    const AirDemand& demand() const { return demand_; }
    bool stopped() const { return stopped_; }
    bool emergency() const { return emergency_; }
    int setCalls() const { return setCalls_; }

private:
    AirDemand demand_{};
    bool stopped_ = false;
    bool emergency_ = false;
    int setCalls_ = 0;
};

// Scriptable transport used to test the composition path.
class MockTransport : public IMidiTransport {
public:
    bool begin() override { return true; }
    void update() override {}
    bool read(MidiEvent& e) override {
        if (queue_.empty()) return false;
        e = queue_.front();
        queue_.erase(queue_.begin());
        return true;
    }
    bool connected() const override { return connected_; }
    const char* name() const override { return "mock"; }

    void push(const MidiEvent& e) { queue_.push_back(e); }
    void setConnected(bool c) { connected_ = c; }

private:
    std::vector<MidiEvent> queue_;
    bool connected_ = true;
};

}  // namespace melodica

#endif  // TEST_MOCKS_H
