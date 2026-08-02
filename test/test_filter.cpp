#include "MidiFilter.h"
#include "native_test.h"

using namespace melodica;

void run_filter() {
    // Channel filtering: Single.
    {
        MidiFilterConfig c;
        c.channelMode = ChannelMode::Single;
        c.singleChannel = 3;
        MidiFilter f(c);
        CHECK(f.acceptsChannel(3));
        CHECK(!f.acceptsChannel(4));
    }
    // Channel filtering: List (mask).
    {
        MidiFilterConfig c;
        c.channelMode = ChannelMode::List;
        c.channelMask = (1u << 1) | (1u << 5);
        MidiFilter f(c);
        CHECK(f.acceptsChannel(1));
        CHECK(f.acceptsChannel(5));
        CHECK(!f.acceptsChannel(2));
    }
    // Omni accepts everything.
    {
        MidiFilterConfig c;
        c.channelMode = ChannelMode::Omni;
        MidiFilter f(c);
        for (uint8_t ch = 0; ch < 16; ++ch) CHECK(f.acceptsChannel(ch));
    }
    // Transpose + range limits.
    {
        MidiFilterConfig c;
        c.transpose = 12;      // up one octave
        c.lowNote = 60;
        c.highNote = 90;
        MidiFilter f(c);
        uint8_t out = 0;
        CHECK(f.mapNote(60, out));   // 60+12 = 72 within range
        CHECK_EQ(out, 72);
        CHECK(!f.mapNote(40, out));  // 40+12 = 52 below lowNote
        CHECK(!f.mapNote(90, out));  // 90+12 = 102 above highNote
    }
    // Transpose that pushes below 0 is rejected.
    {
        MidiFilterConfig c;
        c.transpose = -24;
        MidiFilter f(c);
        uint8_t out = 0;
        CHECK(!f.mapNote(10, out));
    }
}
