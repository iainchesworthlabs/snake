// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "platform/sound_sink.hpp"
#include "snake/audio/tunes.hpp"

#include <QString>
#include <cstdint>
#include <memory>
#include <span>

namespace snake::ui
{

/// The platform service that takes the samples of libs/audio to the speakers. It opens the default audio device
/// through miniaudio, which asks for samples from a thread of its own, so everything that touches the synthesiser is
/// done under a lock: play() may be called from the user interface while the device is asking for samples.
class AudioOutput final : public SoundSink
{
public:
    /// Which device start() opens.
    enum class Device : std::uint8_t
    {
        SYSTEM, // the default audio output of the computer or phone
        NONE,   // a device that plays nothing but asks for samples at the speed of a real one, for tests
    };

    /// The rate the synthesiser works at. The device converts it to its own.
    static constexpr std::uint32_t SAMPLE_RATE = 44100;

    AudioOutput();
    ~AudioOutput() override;

    /// Opens `device` and starts asking for samples. Returns false, leaving the game silent, when no device can be
    /// opened. Does nothing, and returns true, when it is running already.
    [[nodiscard]] bool start(Device device = Device::SYSTEM);
    /// Closes the device. The sounds that were playing are dropped.
    void stop();
    [[nodiscard]] bool running() const;
    /// The name of the audio system in use ("PulseAudio", "WASAPI", "Core Audio" and so on), or empty when stopped.
    [[nodiscard]] QString backend_name() const;

    void play(audio::Effect effect) override;
    void play_music(audio::Tune tune) override;
    void stop_music() override;

    /// True when no effect is sounding.
    [[nodiscard]] bool effects_idle() const;
    [[nodiscard]] bool music_playing() const;

    /// Writes the next samples, as the audio device's callback does. Public so that tests can listen without a device.
    void render(std::span<std::int16_t> out);

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

} // namespace snake::ui
