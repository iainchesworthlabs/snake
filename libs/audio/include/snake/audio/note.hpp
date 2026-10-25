// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <cstdint>

namespace snake::audio
{

/// The MIDI number of a note: 60 is middle C, 69 is the A of 440 hertz. 0 (a note too low to hear on a speaker) stands
/// for a rest where a tune asks for a note. The highest number a frequency is given for is 127.
inline constexpr std::uint8_t REST = 0;
inline constexpr std::uint8_t MAX_NOTE = 127;

/// The frequency of a note in hertz times 256, in equal temperament. Whole numbers only, so a note has the same
/// frequency, and a tune the same samples, on every platform. Notes above MAX_NOTE give the frequency of MAX_NOTE.
///
/// The frequency of note 0 (8.1758 hertz) times 256 is multiplied by the ratio of the semitone to C, in 16-bit
/// fixed point, and doubled once for each octave. The error is below 0.02 percent.
[[nodiscard]] constexpr std::uint32_t note_frequency_q8(std::uint8_t note) noexcept
{
    constexpr std::uint64_t NOTE_0_Q8 = 2093;
    constexpr std::array<std::uint32_t, 12> SEMITONE_Q16 = {65536, 69433, 73562,  77936,  82570,  87480,
                                                            92682, 98193, 104032, 110218, 116772, 123715};
    const std::uint8_t clamped = note > MAX_NOTE ? MAX_NOTE : note;
    // The octaves are applied before the division, so the low bits of the ratio are not lost.
    return static_cast<std::uint32_t>(((NOTE_0_Q8 * SEMITONE_Q16[clamped % 12U]) << (clamped / 12U)) >> 16U);
}

} // namespace snake::audio
