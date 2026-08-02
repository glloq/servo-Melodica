#include "native_test.h"

// Forward declarations of every test suite.
void run_pca_mapping();
void run_air_policy();
void run_air_ramp();
void run_midi_parser();
void run_event_queue();
void run_filter();
void run_keydriver_limits();
void run_calibration_migration();
void run_connection();
void run_instrument_notes();
void run_instrument_velocity_zero();
void run_instrument_cc7_zero();
void run_instrument_chord_and_loudest_release();
void run_instrument_sustain();
void run_instrument_all_notes_off();
void run_instrument_multichannel();
void run_instrument_precharge_nonblocking();
void run_instrument_transport_loss();
void run_air_module_end_to_end();
void run_safety_i2c_fault();
void run_safety_stuck_key();
void run_safety_comm_timeout();

int main() {
    RUN(run_pca_mapping);
    RUN(run_air_policy);
    RUN(run_air_ramp);
    RUN(run_midi_parser);
    RUN(run_event_queue);
    RUN(run_filter);
    RUN(run_keydriver_limits);
    RUN(run_calibration_migration);
    RUN(run_connection);
    RUN(run_instrument_notes);
    RUN(run_instrument_velocity_zero);
    RUN(run_instrument_cc7_zero);
    RUN(run_instrument_chord_and_loudest_release);
    RUN(run_instrument_sustain);
    RUN(run_instrument_all_notes_off);
    RUN(run_instrument_multichannel);
    RUN(run_instrument_precharge_nonblocking);
    RUN(run_instrument_transport_loss);
    RUN(run_air_module_end_to_end);
    RUN(run_safety_i2c_fault);
    RUN(run_safety_stuck_key);
    RUN(run_safety_comm_timeout);

    std::printf("\n%d checks, %d failures\n", nt::checks(), nt::failures());
    return nt::failures() == 0 ? 0 : 1;
}
