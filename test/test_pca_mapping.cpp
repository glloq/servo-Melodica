#include "PcaMapping.h"
#include "native_test.h"

using namespace melodica;

// The four canonical mapping cases from the specification: 16 channels/board.
void run_pca_mapping() {
    CHECK_EQ(CHANNELS_PER_PCA9685, 16);

    PcaLocation s0 = pcaLocationForServo(0);
    CHECK_EQ(s0.driverIndex, 0);
    CHECK_EQ(s0.channel, 0);

    PcaLocation s15 = pcaLocationForServo(15);
    CHECK_EQ(s15.driverIndex, 0);
    CHECK_EQ(s15.channel, 15);

    PcaLocation s16 = pcaLocationForServo(16);
    CHECK_EQ(s16.driverIndex, 1);
    CHECK_EQ(s16.channel, 0);

    PcaLocation s31 = pcaLocationForServo(31);
    CHECK_EQ(s31.driverIndex, 1);
    CHECK_EQ(s31.channel, 15);

    // Board-count helper.
    CHECK_EQ(pcaBoardsRequired(1), 1);
    CHECK_EQ(pcaBoardsRequired(16), 1);
    CHECK_EQ(pcaBoardsRequired(17), 2);
    CHECK_EQ(pcaBoardsRequired(32), 2);
    CHECK_EQ(pcaBoardsRequired(33), 3);
}
