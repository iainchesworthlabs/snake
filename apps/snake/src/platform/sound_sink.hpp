// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/audio/tunes.hpp"

namespace snake::ui
{

/// Where the game sends its sounds. The platform's audio output is one; the tests have a recording one, and the game
/// runs without sound when it has none.
class SoundSink
{
public:
    SoundSink() = default;
    SoundSink(const SoundSink&) = delete;
    SoundSink& operator=(const SoundSink&) = delete;
    SoundSink(SoundSink&&) = delete;
    SoundSink& operator=(SoundSink&&) = delete;
    virtual ~SoundSink() = default;

    /// Plays an effect from its start, over whatever is sounding.
    virtual void play(audio::Effect effect) = 0;
    /// Starts a tune from its beginning in place of the one playing.
    virtual void play_music(audio::Tune tune) = 0;
    virtual void stop_music() = 0;
    /// How loud everything is, as a gain on the samples: 0 for silence, 1 for the full level of the synthesiser. A sink
    /// that cannot change its loudness ignores it.
    virtual void set_volume(float /*gain*/) {}
};

/// The sink the game plays its sounds on: the one the program set, or none, in which case the game is silent. Set it
/// before the user interface is created and clear it after. It is not thread safe.
[[nodiscard]] SoundSink* default_sound_sink() noexcept;
void set_default_sound_sink(SoundSink* sink) noexcept;

} // namespace snake::ui
