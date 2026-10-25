// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/audio/tunes.hpp"

#include <array>

namespace snake::audio
{

namespace
{

constexpr std::uint8_t C4 = 60;
constexpr std::uint8_t D4 = 62;
constexpr std::uint8_t E4 = 64;
constexpr std::uint8_t G4 = 67;
constexpr std::uint8_t A4 = 69;
constexpr std::uint8_t C5 = 72;
constexpr std::uint8_t D5 = 74;
constexpr std::uint8_t E5 = 76;
constexpr std::uint8_t G5 = 79;
constexpr std::uint8_t A5 = 81;
constexpr std::uint8_t C3 = 48;
constexpr std::uint8_t F3 = 53;
constexpr std::uint8_t G3 = 55;
constexpr std::uint8_t A3 = 57;

// The effects. The notes are MIDI numbers; an effect with two notes of one value holds the pitch.
constexpr std::array<Segment, 2> EAT = {{
    {Waveform::SQUARE, 72, 79, 50, 200, 128},
    {Waveform::SQUARE, 79, 91, 70, 200, 128},
}};

constexpr std::array<Segment, 4> BONUS = {{
    {Waveform::SQUARE, 76, 76, 60, 190, 64},
    {Waveform::SQUARE, 80, 80, 60, 190, 64},
    {Waveform::SQUARE, 83, 83, 60, 190, 64},
    {Waveform::SQUARE, 88, 88, 140, 190, 64},
}};

constexpr std::array<Segment, 2> CRASH = {{
    {Waveform::TRIANGLE, 52, 28, 140, 230, 128},
    {Waveform::NOISE, 88, 64, 300, 220, 128},
}};

constexpr std::array<Segment, 1> SELECT = {{
    {Waveform::SQUARE, 84, 84, 30, 150, 128},
}};

constexpr std::array<Segment, 4> START = {{
    {Waveform::SQUARE, 72, 72, 70, 180, 128},
    {Waveform::SQUARE, 76, 76, 70, 180, 128},
    {Waveform::SQUARE, 79, 79, 70, 180, 128},
    {Waveform::SQUARE, 84, 84, 160, 180, 128},
}};

constexpr std::array<Segment, 6> NEW_BEST = {{
    {Waveform::SQUARE, 72, 72, 80, 190, 64},
    {Waveform::SQUARE, 76, 76, 80, 190, 64},
    {Waveform::SQUARE, 79, 79, 80, 190, 64},
    {Waveform::SQUARE, 84, 84, 80, 190, 64},
    {Waveform::SQUARE, 79, 79, 80, 190, 64},
    {Waveform::SQUARE, 88, 88, 260, 190, 64},
}};

// The title tune: 16 steps, a bouncing melody over a walking bass.
constexpr std::array<TuneNote, 8> TITLE_LEAD = {
    {{C5, 2}, {E5, 2}, {G5, 2}, {E5, 2}, {A5, 2}, {G5, 2}, {E5, 2}, {D5, 2}}};
constexpr std::array<TuneNote, 8> TITLE_HARMONY = {
    {{E4, 2}, {G4, 2}, {C5, 2}, {G4, 2}, {C5, 2}, {C5, 2}, {G4, 2}, {G4, 2}}};
constexpr std::array<TuneNote, 5> TITLE_BASS = {{{C3, 4}, {G3, 2}, {C3, 2}, {F3, 4}, {G3, 4}}};
constexpr std::array<Channel, 3> TITLE_CHANNELS = {{
    {Waveform::SQUARE, 64, 110, TITLE_LEAD},
    {Waveform::SQUARE, 128, 70, TITLE_HARMONY},
    {Waveform::TRIANGLE, 128, 150, TITLE_BASS},
}};

// The game tune: 32 steps, steadier and lower so that it can run under the effects for a long time.
constexpr std::array<TuneNote, 12> GAME_LEAD = {
    {{E5, 2}, {REST, 2}, {D5, 2}, {C5, 2}, {D5, 4}, {REST, 4}, {E5, 2}, {G5, 2}, {E5, 2}, {D5, 2}, {C5, 4}, {A4, 4}}};
constexpr std::array<TuneNote, 8> GAME_HARMONY = {
    {{C4, 4}, {E4, 4}, {D4, 4}, {G4, 4}, {C4, 4}, {E4, 4}, {A4, 4}, {G4, 4}}};
constexpr std::array<TuneNote, 8> GAME_BASS = {
    {{C3, 4}, {C3, 4}, {G3, 4}, {G3, 4}, {A3, 4}, {A3, 4}, {F3, 4}, {G3, 4}}};
constexpr std::array<Channel, 3> GAME_CHANNELS = {{
    {Waveform::SQUARE, 32, 100, GAME_LEAD},
    {Waveform::SQUARE, 128, 55, GAME_HARMONY},
    {Waveform::TRIANGLE, 128, 140, GAME_BASS},
}};

} // namespace

std::span<const Segment> effect_segments(Effect effect) noexcept
{
    switch (effect)
    {
    case Effect::EAT:
        return EAT;
    case Effect::BONUS:
        return BONUS;
    case Effect::CRASH:
        return CRASH;
    case Effect::SELECT:
        return SELECT;
    case Effect::START:
        return START;
    case Effect::NEW_BEST:
        return NEW_BEST;
    }
    return {};
}

TuneData tune_data(Tune tune) noexcept
{
    switch (tune)
    {
    case Tune::TITLE:
        return {.step_ms = 150, .channels = TITLE_CHANNELS};
    case Tune::GAME:
        return {.step_ms = 140, .channels = GAME_CHANNELS};
    }
    return {};
}

} // namespace snake::audio
