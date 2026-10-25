// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/audio/tunes.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace snake::audio
{

/// The sound of the game, made from numbers: square, triangle and noise voices, in whole-number arithmetic, so a
/// given sequence of calls gives the same samples on every platform. It plays the effects of Effect and the tunes of
/// Tune and writes 16-bit mono samples. It does not know how they reach a speaker; the platform asks for samples when
/// its audio output needs them. It allocates nothing, uses no clock and is not thread safe: the caller serialises the
/// calls to play() and render().
class Synth
{
public:
    /// How many effects can sound at once. A new one takes the voice that has played longest when all are busy.
    static constexpr std::size_t EFFECT_VOICES = 4;

    /// `sample_rate` in samples per second, 8000 to 96000.
    explicit Synth(std::uint32_t sample_rate) noexcept;

    [[nodiscard]] std::uint32_t sample_rate() const noexcept
    {
        return m_SampleRate;
    }

    /// Starts `effect` from its beginning.
    void play(Effect effect) noexcept;

    /// Starts `tune` from its beginning, replacing the tune that played.
    void play_music(Tune tune) noexcept;
    void stop_music() noexcept;

    [[nodiscard]] bool music_playing() const noexcept
    {
        return m_MusicPlaying;
    }

    /// True when no effect is sounding.
    [[nodiscard]] bool effects_idle() const noexcept;

    /// Stops every effect and the music at once.
    void stop_all() noexcept;

    /// Writes the next `out.size()` samples. Rendering in pieces gives the same samples as rendering at once.
    void render(std::span<std::int16_t> out) noexcept;

private:
    /// A wave generator: a 32-bit phase that wraps once a period.
    struct Oscillator
    {
        Waveform waveform = Waveform::SQUARE;
        std::uint8_t duty = 128;
        std::uint32_t phase = 0;
        std::uint32_t increment = 0;
        std::uint16_t shift_register = 1;
        std::int32_t noise_level = 32767;

        void set(Waveform wave, std::uint8_t duty_cycle) noexcept;
        void tune_to(std::uint32_t frequency_q8, std::uint32_t sample_rate) noexcept;
        [[nodiscard]] std::int32_t next() noexcept;
    };

    struct EffectVoice
    {
        bool active = false;
        std::span<const Segment> segments;
        std::size_t segment = 0;
        std::uint32_t position = 0;
        std::uint32_t length = 0;
        std::uint32_t started = 0; // the order effects began in, to find the oldest
        Oscillator oscillator;
    };

    struct MusicChannel
    {
        const Channel* channel = nullptr;
        std::size_t note = 0;
        std::uint32_t position = 0;
        std::uint32_t length = 0;
        Oscillator oscillator;
    };

    void begin_segment(EffectVoice& voice) noexcept;
    void begin_note(MusicChannel& channel) noexcept;
    [[nodiscard]] std::int32_t next_effect_sample(EffectVoice& voice) noexcept;
    [[nodiscard]] std::int32_t next_music_sample(MusicChannel& channel) noexcept;

    std::uint32_t m_SampleRate;
    std::uint32_t m_Plays = 0;
    std::array<EffectVoice, EFFECT_VOICES> m_Voices;
    std::array<MusicChannel, MAX_CHANNELS> m_Music;
    std::size_t m_MusicChannels = 0;
    std::uint32_t m_StepSamples = 0;
    bool m_MusicPlaying = false;
};

} // namespace snake::audio
