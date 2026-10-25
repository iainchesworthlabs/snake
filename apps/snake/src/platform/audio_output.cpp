// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform/audio_output.hpp"

#include "snake/audio/synth.hpp"
#include "snake/audio/tunes.hpp"

#include <miniaudio.h>

#include <QString>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <span>

namespace snake::ui
{

struct AudioOutput::Impl
{
    /// Guards the synthesiser and the flag. It is never held while the device is started or stopped, because the
    /// device's thread takes it in the callback and stopping the device waits for that thread.
    mutable std::mutex lock;
    audio::Synth synth{SAMPLE_RATE};
    bool running = false;
    ma_context context{};
    ma_device device{};
};

namespace
{

// miniaudio calls this from the audio thread whenever the device wants more samples.
void fill_device_buffer(ma_device* device, void* output, const void* /*input*/, ma_uint32 frame_count)
{
    auto* owner = static_cast<AudioOutput*>(device->pUserData);
    owner->render({static_cast<std::int16_t*>(output), static_cast<std::size_t>(frame_count)});
}

} // namespace

AudioOutput::AudioOutput() : m_Impl(std::make_unique<Impl>()) {}

AudioOutput::~AudioOutput()
{
    stop();
}

bool AudioOutput::start(Device device)
{
    if (running())
    {
        return true;
    }

    const ma_backend null_backend = ma_backend_null;
    const ma_result context_result = device == Device::NONE
                                         ? ma_context_init(&null_backend, 1, nullptr, &m_Impl->context)
                                         : ma_context_init(nullptr, 0, nullptr, &m_Impl->context);
    if (context_result != MA_SUCCESS)
    {
        return false;
    }

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_s16;
    config.playback.channels = 1;
    config.sampleRate = SAMPLE_RATE;
    config.dataCallback = &fill_device_buffer;
    config.pUserData = this;
    if (ma_device_init(&m_Impl->context, &config, &m_Impl->device) != MA_SUCCESS)
    {
        ma_context_uninit(&m_Impl->context);
        return false;
    }
    if (ma_device_start(&m_Impl->device) != MA_SUCCESS)
    {
        ma_device_uninit(&m_Impl->device);
        ma_context_uninit(&m_Impl->context);
        return false;
    }

    const std::lock_guard guard(m_Impl->lock);
    m_Impl->running = true;
    return true;
}

void AudioOutput::stop()
{
    {
        const std::lock_guard guard(m_Impl->lock);
        if (!m_Impl->running)
        {
            return;
        }
        m_Impl->running = false;
    }
    // Waits for the audio thread, which may be in render() for the lock: so the lock is not held here.
    ma_device_uninit(&m_Impl->device);
    ma_context_uninit(&m_Impl->context);
    const std::lock_guard guard(m_Impl->lock);
    m_Impl->synth.stop_all();
}

bool AudioOutput::running() const
{
    const std::lock_guard guard(m_Impl->lock);
    return m_Impl->running;
}

QString AudioOutput::backend_name() const
{
    const std::lock_guard guard(m_Impl->lock);
    return m_Impl->running ? QString::fromUtf8(ma_get_backend_name(m_Impl->context.backend)) : QString();
}

void AudioOutput::play(audio::Effect effect)
{
    const std::lock_guard guard(m_Impl->lock);
    m_Impl->synth.play(effect);
}

void AudioOutput::play_music(audio::Tune tune)
{
    const std::lock_guard guard(m_Impl->lock);
    m_Impl->synth.play_music(tune);
}

void AudioOutput::stop_music()
{
    const std::lock_guard guard(m_Impl->lock);
    m_Impl->synth.stop_music();
}

bool AudioOutput::effects_idle() const
{
    const std::lock_guard guard(m_Impl->lock);
    return m_Impl->synth.effects_idle();
}

bool AudioOutput::music_playing() const
{
    const std::lock_guard guard(m_Impl->lock);
    return m_Impl->synth.music_playing();
}

void AudioOutput::render(std::span<std::int16_t> out)
{
    const std::lock_guard guard(m_Impl->lock);
    m_Impl->synth.render(out);
}

} // namespace snake::ui
