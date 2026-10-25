// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform/audio_output.hpp"
#include "platform/sound_sink.hpp"
#include "snake/audio/tunes.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

using snake::audio::Effect;
using snake::audio::Tune;
using snake::ui::AudioOutput;
using snake::ui::SoundSink;

namespace
{

std::vector<std::int16_t> listen(AudioOutput& output, std::size_t count)
{
    std::vector<std::int16_t> samples(count);
    output.render(samples);
    return samples;
}

bool silent(const std::vector<std::int16_t>& samples)
{
    return std::ranges::all_of(samples, [](std::int16_t sample) { return sample == 0; });
}

/// Waits until `done` is true, for at most `seconds`.
template <typename Done>
bool wait_for(Done done, double seconds)
{
    const auto end = std::chrono::steady_clock::now() + std::chrono::duration<double>(seconds);
    while (!done())
    {
        if (std::chrono::steady_clock::now() > end)
        {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return true;
}

} // namespace

TEST_CASE("An output that has not been started is silent until it is given a sound", "[audio-output]")
{
    AudioOutput output;
    CHECK_FALSE(output.running());
    CHECK(output.backend_name().isEmpty());
    CHECK(output.effects_idle());
    CHECK_FALSE(output.music_playing());
    CHECK(silent(listen(output, 500)));

    output.play(Effect::EAT);
    CHECK_FALSE(output.effects_idle());
    CHECK_FALSE(silent(listen(output, 500)));
}

TEST_CASE("Music plays until it is stopped", "[audio-output]")
{
    AudioOutput output;
    output.play_music(Tune::GAME);
    CHECK(output.music_playing());
    CHECK_FALSE(silent(listen(output, 4000)));
    output.stop_music();
    CHECK_FALSE(output.music_playing());
    CHECK(silent(listen(output, 500)));
}

TEST_CASE("It is a sound sink, so the game does not need to know the platform", "[audio-output]")
{
    AudioOutput output;
    SoundSink& sink = output;
    sink.play(Effect::SELECT);
    sink.play_music(Tune::TITLE);
    CHECK_FALSE(output.effects_idle());
    CHECK(output.music_playing());
    sink.stop_music();
    CHECK_FALSE(output.music_playing());
}

TEST_CASE("A device that plays nothing is asked for samples as a real one is", "[audio-output]")
{
    AudioOutput output;
    REQUIRE(output.start(AudioOutput::Device::NONE));
    CHECK(output.running());
    CHECK_FALSE(output.backend_name().isEmpty());
    // Starting again changes nothing.
    CHECK(output.start(AudioOutput::Device::NONE));

    // The effect lasts 120 ms. It is over only if the device's thread asked for the samples.
    output.play(Effect::EAT);
    CHECK(wait_for([&] { return output.effects_idle(); }, 5.0));

    output.stop();
    CHECK_FALSE(output.running());
    CHECK(output.backend_name().isEmpty());
    // Stopping twice is fine, and it can start again.
    output.stop();
    CHECK(output.start(AudioOutput::Device::NONE));
}

TEST_CASE("Stopping the device drops the sounds that were playing", "[audio-output]")
{
    AudioOutput output;
    REQUIRE(output.start(AudioOutput::Device::NONE));
    output.play_music(Tune::GAME);
    output.play(Effect::NEW_BEST);
    output.stop();
    CHECK(output.effects_idle());
    CHECK_FALSE(output.music_playing());
}

TEST_CASE("The user interface can play sounds while the device asks for samples", "[audio-output]")
{
    AudioOutput output;
    REQUIRE(output.start(AudioOutput::Device::NONE));
    std::atomic<bool> stop{false};
    std::thread player(
        [&]
        {
            while (!stop)
            {
                output.play(Effect::SELECT);
                output.play_music(Tune::TITLE);
                output.stop_music();
            }
        });
    // Both sides take the lock a few hundred times.
    std::vector<std::int16_t> buffer(256);
    for (int i = 0; i < 300; ++i)
    {
        output.render(buffer);
    }
    stop = true;
    player.join();
    output.stop();
    SUCCEED();
}

TEST_CASE("The default device either opens or is reported as missing", "[audio-output]")
{
    // A machine without a sound card (a build server) has no device to open, or only the null one: both are fine. The
    // game goes on in silence when it cannot.
    AudioOutput output;
    if (output.start(AudioOutput::Device::SYSTEM))
    {
        CHECK(output.running());
        CHECK_FALSE(output.backend_name().isEmpty());
        output.stop();
    }
    else
    {
        CHECK_FALSE(output.running());
    }
}
