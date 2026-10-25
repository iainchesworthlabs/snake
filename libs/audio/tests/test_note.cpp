// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/audio/note.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

using snake::audio::MAX_NOTE;
using snake::audio::note_frequency_q8;

TEST_CASE("The A above middle C is 440 hertz", "[note]")
{
    const std::uint32_t frequency = note_frequency_q8(69);
    // 440 hertz times 256, within 0.05 percent.
    CHECK(frequency >= 112'540);
    CHECK(frequency <= 112'700);
}

TEST_CASE("Middle C is 261.6 hertz", "[note]")
{
    const std::uint32_t frequency = note_frequency_q8(60);
    CHECK(frequency >= 66'900);
    CHECK(frequency <= 67'040);
}

TEST_CASE("An octave up doubles the frequency, to the last bit", "[note]")
{
    for (std::uint8_t note = 0; note + 12 <= MAX_NOTE; ++note)
    {
        INFO("note " << static_cast<int>(note));
        // The lower note is rounded down once and the higher one once, so the higher may be a bit above double.
        const std::uint32_t doubled = note_frequency_q8(note) * 2;
        const std::uint32_t octave_up = note_frequency_q8(static_cast<std::uint8_t>(note + 12));
        CHECK(octave_up >= doubled);
        CHECK(octave_up <= doubled + 1);
    }
}

TEST_CASE("Each note is higher than the one below it", "[note]")
{
    for (std::uint8_t note = 1; note <= MAX_NOTE; ++note)
    {
        INFO("note " << static_cast<int>(note));
        CHECK(note_frequency_q8(note) > note_frequency_q8(static_cast<std::uint8_t>(note - 1)));
    }
}

TEST_CASE("Every semitone is about 5.9 percent (the twelfth root of two) above the one below it", "[note]")
{
    for (std::uint8_t note = 40; note < 100; ++note)
    {
        const double ratio = static_cast<double>(note_frequency_q8(static_cast<std::uint8_t>(note + 1))) /
                             static_cast<double>(note_frequency_q8(note));
        INFO("note " << static_cast<int>(note));
        CHECK(ratio > 1.0588);
        CHECK(ratio < 1.0601);
    }
}

TEST_CASE("A note above the highest gives the frequency of the highest", "[note]")
{
    CHECK(note_frequency_q8(200) == note_frequency_q8(MAX_NOTE));
    CHECK(note_frequency_q8(255) == note_frequency_q8(MAX_NOTE));
}
