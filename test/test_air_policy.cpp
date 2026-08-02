#include "AirPolicy.h"
#include "native_test.h"

using namespace melodica;

void run_air_policy() {
    const uint8_t full = 127;

    // No notes => zero demand regardless of controllers.
    CHECK_EQ(computeAirExpression(AirPolicy::MaximumVelocity, nullptr, 0, full, full), 0);

    // CC7 == 0 => air closed even with an active note.
    uint8_t one[] = {100};
    CHECK_EQ(computeAirExpression(AirPolicy::MaximumVelocity, one, 1, 0, full), 0);

    // CC11 == 0 => air closed.
    CHECK_EQ(computeAirExpression(AirPolicy::MaximumVelocity, one, 1, full, 0), 0);

    // MaximumVelocity follows the loudest note.
    uint8_t chord[] = {40, 100, 60};
    uint8_t maxDemand = computeAirExpression(AirPolicy::MaximumVelocity, chord, 3, full, full);
    uint8_t soloDemand = computeAirExpression(AirPolicy::MaximumVelocity, one, 1, full, full);
    CHECK(maxDemand >= soloDemand);  // 100 vs 100 => equal
    CHECK(maxDemand > 0);

    // Stopping the loudest note lowers the demand (soft note remains).
    uint8_t afterLoudStops[] = {40, 60};
    uint8_t before = computeAirExpression(AirPolicy::MaximumVelocity, chord, 3, full, full);
    uint8_t after = computeAirExpression(AirPolicy::MaximumVelocity, afterLoudStops, 2, full, full);
    CHECK(after < before);

    // Average is between min and max velocities.
    uint8_t avg = computeAirExpression(AirPolicy::AverageVelocity, chord, 3, full, full);
    CHECK(avg > 0 && avg < maxDemand);

    // SumClamped never exceeds full scale.
    uint8_t loud[] = {127, 127, 127};
    CHECK_EQ(computeAirExpression(AirPolicy::SumClamped, loud, 3, full, full), 255);

    // Polyphony compensation raises demand as notes are added at equal velocity.
    uint8_t p1[] = {80};
    uint8_t p3[] = {80, 80, 80};
    uint8_t d1 = computeAirExpression(AirPolicy::PolyphonyCompensated, p1, 1, full, full);
    uint8_t d3 = computeAirExpression(AirPolicy::PolyphonyCompensated, p3, 3, full, full);
    CHECK(d3 > d1);

    // Volume scaling: half volume roughly halves demand.
    uint8_t fullVol = computeAirExpression(AirPolicy::Fixed, one, 1, 127, 127);
    uint8_t halfVol = computeAirExpression(AirPolicy::Fixed, one, 1, 64, 127);
    CHECK(halfVol < fullVol);
}
