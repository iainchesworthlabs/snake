// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/audio/synth.hpp"

#include "snake/audio/note.hpp"
#include "snake/audio/tunes.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

namespace snake::audio
{

namespace
{

constexpr std::uint32_t MIN_SAMPLE_RATE = 8000;
constexpr std::uint32_t MAX_SAMPLE_RATE = 96000;
constexpr std::int32_t PEAK = 32767;

/// How long a sound takes to rise from silence, so that it starts without a click.
constexpr std::uint32_t ATTACK_MS = 2;

/// The effects are mixed at half their level and the music at a quarter, so that the effects stay clear of the music
/// and all of them together stay inside 16 bits.
constexpr std::int32_t EFFECT_SHIFT = 1;
constexpr std::int32_t MUSIC_SHIFT = 2;

/// How often (in samples) a sliding note is retuned.
constexpr std::uint32_t RETUNE_MASK = 15;

std::uint32_t samples_for(std::uint32_t milliseconds, std::uint32_t sample_rate) noexcept
{
    return static_cast<std::uint32_t>(static_cast<std::uint64_t>(milliseconds) * sample_rate / 1000U);
}

/// The frequency, times 256, of the note `note_q8` / 256, between the frequencies of the two notes around it.
std::uint32_t slide_frequency_q8(std::uint32_t note_q8) noexcept
{
    const auto whole = static_cast<std::uint8_t>(std::min<std::uint32_t>(note_q8 >> 8U, MAX_NOTE));
    const std::uint32_t low = note_frequency_q8(whole);
    const std::uint32_t high = note_frequency_q8(static_cast<std::uint8_t>(whole + 1U));
    // At the highest note both are the same frequency, so the slide has nowhere to go.
    return low + static_cast<std::uint32_t>(static_cast<std::uint64_t>(high - low) * (note_q8 & 255U) / 256U);
}

/// A level from 0 to 65535: a rise over `attack` samples, then a fall to silence at the end of `length`.
std::uint32_t fade_out(std::uint32_t position, std::uint32_t length, std::uint32_t attack) noexcept
{
    // The rise is at most a quarter of the length, so the fall that follows is never empty.
    const std::uint32_t rise = std::min(attack, length / 4U);
    if (position < rise)
    {
        return static_cast<std::uint32_t>(static_cast<std::uint64_t>(position) * 65535U / rise);
    }
    return static_cast<std::uint32_t>(static_cast<std::uint64_t>(length - position) * 65535U / (length - rise));
}

/// A level from 0 to 65535 for a note of a tune: a rise, full level, and a fall over the last fifth so that notes
/// of the same pitch are told apart.
std::uint32_t note_level(std::uint32_t position, std::uint32_t length, std::uint32_t attack) noexcept
{
    const std::uint32_t rise = std::min(attack, length / 4U);
    if (position < rise)
    {
        return static_cast<std::uint32_t>(static_cast<std::uint64_t>(position) * 65535U / rise);
    }
    const std::uint32_t release = length / 5U;
    if (position + release > length)
    {
        return static_cast<std::uint32_t>(static_cast<std::uint64_t>(length - position) * 65535U / release);
    }
    return 65535U;
}

std::int32_t scale(std::int32_t sample, std::uint32_t volume, std::uint32_t level) noexcept
{
    const std::int64_t product = static_cast<std::int64_t>(sample) * volume;
    return static_cast<std::int32_t>((product / 256) * static_cast<std::int64_t>(level) / 65536);
}

} // namespace

void Synth::Oscillator::set(Waveform wave, std::uint8_t duty_cycle) noexcept
{
    waveform = wave;
    duty = duty_cycle;
    phase = 0;
    shift_register = 1;
    noise_level = PEAK;
}

void Synth::Oscillator::tune_to(std::uint32_t frequency_q8, std::uint32_t sample_rate) noexcept
{
    // increment = frequency / sample_rate * 2^32, and frequency_q8 is frequency * 2^8.
    increment = static_cast<std::uint32_t>((static_cast<std::uint64_t>(frequency_q8) << 24U) / sample_rate);
}

std::int32_t Synth::Oscillator::next() noexcept
{
    const std::uint32_t before = phase;
    phase += increment;
    if (waveform == Waveform::SQUARE)
    {
        return (phase >> 24U) < duty ? PEAK : -PEAK;
    }
    if (waveform == Waveform::TRIANGLE)
    {
        const auto level = static_cast<std::int32_t>(phase >> 16U);
        return (level < 32768 ? level : 65535 - level) * 2 - 32768;
    }
    // Noise: a shift register that moves on once a period.
    if (phase < before)
    {
        const auto feedback = static_cast<std::uint16_t>((shift_register ^ (shift_register >> 1U)) & 1U);
        shift_register = static_cast<std::uint16_t>((shift_register >> 1U) | (feedback << 14U));
        noise_level = (shift_register & 1U) != 0 ? PEAK : -PEAK;
    }
    return noise_level;
}

Synth::Synth(std::uint32_t sample_rate) noexcept
    : m_SampleRate(std::clamp(sample_rate, MIN_SAMPLE_RATE, MAX_SAMPLE_RATE))
{
}

bool Synth::effects_idle() const noexcept
{
    return std::ranges::none_of(m_Voices, [](const EffectVoice& voice) { return voice.active; });
}

void Synth::stop_all() noexcept
{
    for (EffectVoice& voice : m_Voices)
    {
        voice.active = false;
    }
    stop_music();
}

void Synth::begin_segment(EffectVoice& voice) noexcept
{
    const Segment& segment = voice.segments[voice.segment];
    voice.position = 0;
    voice.length = samples_for(segment.duration_ms, m_SampleRate);
    voice.oscillator.set(segment.waveform, segment.duty);
    voice.oscillator.tune_to(slide_frequency_q8(static_cast<std::uint32_t>(segment.from_note) << 8U), m_SampleRate);
}

void Synth::play(Effect effect) noexcept
{
    const std::span<const Segment> segments = effect_segments(effect);
    if (segments.empty())
    {
        return;
    }
    EffectVoice* chosen = &m_Voices[0];
    for (EffectVoice& voice : m_Voices)
    {
        if (!voice.active)
        {
            chosen = &voice;
            break;
        }
        // All busy so far: the one that began longest ago is taken.
        if (m_Plays - voice.started > m_Plays - chosen->started)
        {
            chosen = &voice;
        }
    }
    chosen->active = true;
    chosen->segments = segments;
    chosen->segment = 0;
    chosen->started = m_Plays++;
    begin_segment(*chosen);
}

void Synth::begin_note(MusicChannel& channel) noexcept
{
    const TuneNote& note = channel.channel->notes[channel.note];
    channel.position = 0;
    channel.length = static_cast<std::uint32_t>(static_cast<std::uint64_t>(note.steps) * m_StepSamples);
    if (note.note != REST)
    {
        // Every note starts its wave afresh, so a tune that loops repeats exactly.
        channel.oscillator.phase = 0;
        channel.oscillator.tune_to(note_frequency_q8(note.note), m_SampleRate);
    }
}

void Synth::play_music(Tune tune) noexcept
{
    const TuneData data = tune_data(tune);
    m_StepSamples = samples_for(data.step_ms, m_SampleRate);
    m_MusicChannels = std::min(data.channels.size(), MAX_CHANNELS);
    for (std::size_t i = 0; i < m_MusicChannels; ++i)
    {
        MusicChannel& channel = m_Music[i];
        channel.channel = &data.channels[i];
        channel.note = 0;
        channel.oscillator.set(channel.channel->waveform, channel.channel->duty);
        begin_note(channel);
    }
    m_MusicPlaying = m_MusicChannels != 0;
}

void Synth::stop_music() noexcept
{
    m_MusicPlaying = false;
}

std::int32_t Synth::next_effect_sample(EffectVoice& voice) noexcept
{
    while (voice.position >= voice.length)
    {
        ++voice.segment;
        if (voice.segment >= voice.segments.size())
        {
            voice.active = false;
            return 0;
        }
        begin_segment(voice);
    }
    const Segment& segment = voice.segments[voice.segment];
    if ((voice.position & RETUNE_MASK) == 0 && segment.from_note != segment.to_note)
    {
        const std::int64_t from = static_cast<std::int64_t>(segment.from_note) * 256;
        const std::int64_t to = static_cast<std::int64_t>(segment.to_note) * 256;
        const std::int64_t note_q8 = from + (to - from) * static_cast<std::int64_t>(voice.position) / voice.length;
        voice.oscillator.tune_to(slide_frequency_q8(static_cast<std::uint32_t>(note_q8)), m_SampleRate);
    }
    const std::uint32_t level = fade_out(voice.position, voice.length, samples_for(ATTACK_MS, m_SampleRate));
    ++voice.position;
    return scale(voice.oscillator.next(), segment.volume, level);
}

std::int32_t Synth::next_music_sample(MusicChannel& channel) noexcept
{
    if (channel.position >= channel.length)
    {
        channel.note = (channel.note + 1) % channel.channel->notes.size();
        begin_note(channel);
    }
    const TuneNote& note = channel.channel->notes[channel.note];
    const std::uint32_t level = note_level(channel.position, channel.length, samples_for(ATTACK_MS, m_SampleRate));
    ++channel.position;
    if (note.note == REST)
    {
        return 0;
    }
    return scale(channel.oscillator.next(), channel.channel->volume, level);
}

void Synth::render(std::span<std::int16_t> out) noexcept
{
    for (std::int16_t& sample : out)
    {
        std::int32_t effects = 0;
        for (EffectVoice& voice : m_Voices)
        {
            if (voice.active)
            {
                effects += next_effect_sample(voice);
            }
        }
        std::int32_t music = 0;
        if (m_MusicPlaying)
        {
            for (std::size_t i = 0; i < m_MusicChannels; ++i)
            {
                music += next_music_sample(m_Music[i]);
            }
        }
        const std::int32_t mixed = (effects >> EFFECT_SHIFT) + (music >> MUSIC_SHIFT);
        sample = static_cast<std::int16_t>(std::clamp(mixed, -PEAK, PEAK));
    }
}

} // namespace snake::audio
