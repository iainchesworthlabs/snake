// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/audio/note.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace snake::audio
{

/// The shape of a voice's wave. SQUARE has a duty (how much of each period is high), TRIANGLE is soft and used for
/// bass, and NOISE is a shift register clocked at the pitch, for crashes and hisses.
enum class Waveform : std::uint8_t
{
    SQUARE,
    TRIANGLE,
    NOISE,
};

/// One piece of a sound effect: a voice that slides from one note to another over `duration_ms` and fades out as it
/// goes. An effect is a list of these, played one after the other.
struct Segment
{
    Waveform waveform = Waveform::SQUARE;
    std::uint8_t from_note = 0;
    std::uint8_t to_note = 0;
    std::uint16_t duration_ms = 0;
    std::uint8_t volume = 0; // 0 to 255
    std::uint8_t duty = 128; // of a square wave: 128 is half, 64 a quarter
};

/// The sound effects of the game. The first sounds are placeholders to be judged by ear.
enum class Effect : std::uint8_t
{
    EAT,      // the snake eats
    BONUS,    // the snake eats the bonus item
    CRASH,    // the game ends
    SELECT,   // a button is pressed
    START,    // a game starts
    NEW_BEST, // a score makes the leaderboard
};

inline constexpr std::size_t EFFECT_COUNT = 6;

/// The segments of `effect`, in the order they play.
[[nodiscard]] std::span<const Segment> effect_segments(Effect effect) noexcept;

/// One note of a tune, held for `steps` steps. A `note` of REST is silence.
struct TuneNote
{
    std::uint8_t note = REST;
    std::uint8_t steps = 1;
};

/// A voice of a tune: its wave, volume and the notes it plays. All the channels of a tune last the same number of
/// steps.
struct Channel
{
    Waveform waveform = Waveform::SQUARE;
    std::uint8_t duty = 128;
    std::uint8_t volume = 0;
    std::span<const TuneNote> notes;
};

/// The tunes of the game. They are placeholders to be judged by ear.
enum class Tune : std::uint8_t
{
    TITLE, // while the title is up
    GAME,  // while a game runs
};

inline constexpr std::size_t TUNE_COUNT = 2;

/// The most channels a tune has.
inline constexpr std::size_t MAX_CHANNELS = 3;

/// A tune: the length of a step and the channels. A tune plays again from the start when it ends.
struct TuneData
{
    std::uint16_t step_ms = 0;
    std::span<const Channel> channels;
};

[[nodiscard]] TuneData tune_data(Tune tune) noexcept;

} // namespace snake::audio
