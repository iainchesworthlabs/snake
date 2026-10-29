// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/audio/synth.hpp"
#include "snake/audio/tunes.hpp"
#include "snake/sim/fnv1a.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <numeric>
#include <vector>

using namespace snake::audio;

namespace
{

constexpr std::uint32_t RATE = 22050;

constexpr std::array<Effect, EFFECT_COUNT> EFFECTS = {Effect::EAT,   Effect::BONUS,    Effect::CRASH,  Effect::SELECT,
                                                      Effect::START, Effect::NEW_BEST, Effect::PORTAL, Effect::POTATO,
                                                      Effect::SHED,  Effect::EXPLODE};

std::vector<std::int16_t> render(Synth& synth, std::size_t count)
{
    std::vector<std::int16_t> samples(count);
    synth.render(samples);
    return samples;
}

bool silent(const std::vector<std::int16_t>& samples)
{
    return std::ranges::all_of(samples, [](std::int16_t sample) { return sample == 0; });
}

std::uint64_t hash_of(const std::vector<std::int16_t>& samples)
{
    std::vector<std::uint8_t> bytes;
    bytes.reserve(samples.size() * 2);
    for (const std::int16_t sample : samples)
    {
        const auto value = static_cast<std::uint16_t>(sample);
        bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
        bytes.push_back(static_cast<std::uint8_t>(value >> 8U));
    }
    return snake::sim::fnv1a64(bytes);
}

std::size_t sign_changes(const std::vector<std::int16_t>& samples, std::size_t first, std::size_t last)
{
    std::size_t changes = 0;
    for (std::size_t i = first + 1; i < last; ++i)
    {
        if ((samples[i - 1] < 0) != (samples[i] < 0) && samples[i - 1] != 0 && samples[i] != 0)
        {
            ++changes;
        }
    }
    return changes;
}

int largest_step(const std::vector<std::int16_t>& samples, std::size_t first, std::size_t last)
{
    int largest = 0;
    for (std::size_t i = first + 1; i < last; ++i)
    {
        largest = std::max(largest, std::abs(samples[i] - samples[i - 1]));
    }
    return largest;
}

std::size_t samples_of(Effect effect, std::uint32_t rate)
{
    std::size_t total = 0;
    for (const Segment& segment : effect_segments(effect))
    {
        total += static_cast<std::size_t>(segment.duration_ms) * rate / 1000;
    }
    return total;
}

} // namespace

TEST_CASE("A synth with nothing to play is silent", "[synth]")
{
    Synth synth(RATE);
    CHECK(silent(render(synth, 1000)));
    CHECK(synth.effects_idle());
    CHECK_FALSE(synth.music_playing());
}

TEST_CASE("The sample rate is kept within what the voices can make", "[synth]")
{
    CHECK(Synth(1).sample_rate() == 8000);
    CHECK(Synth(1'000'000).sample_rate() == 96000);
    CHECK(Synth(44100).sample_rate() == 44100);
}

TEST_CASE("An effect sounds for the time of its segments and then stops", "[synth]")
{
    for (const Effect effect : EFFECTS)
    {
        INFO("effect " << static_cast<int>(effect));
        Synth synth(RATE);
        synth.play(effect);
        CHECK_FALSE(synth.effects_idle());
        const std::size_t total = samples_of(effect, RATE);
        const auto samples = render(synth, total - 1);
        CHECK_FALSE(silent(samples));
        CHECK_FALSE(synth.effects_idle());
        render(synth, 2);
        CHECK(synth.effects_idle());
        CHECK(silent(render(synth, 500)));
    }
}

TEST_CASE("An effect lasts as long at any sample rate", "[synth]")
{
    Synth synth(44100);
    synth.play(Effect::EAT);
    const std::size_t total = samples_of(Effect::EAT, 44100);
    CHECK(total > 5000);
    CHECK(total < 5400);
    render(synth, total - 1);
    CHECK_FALSE(synth.effects_idle());
    render(synth, 2);
    CHECK(synth.effects_idle());
}

TEST_CASE("An effect starts and ends without a click", "[synth]")
{
    for (const Effect effect : EFFECTS)
    {
        INFO("effect " << static_cast<int>(effect));
        Synth synth(RATE);
        synth.play(effect);
        const auto samples = render(synth, samples_of(effect, RATE));
        // The first sample is silence and the wave rises from it over a couple of milliseconds.
        CHECK(std::abs(samples.front()) < 600);
        CHECK(std::abs(samples.back()) < 600);
    }
}

TEST_CASE("Rendering in pieces gives the same samples as rendering at once", "[synth]")
{
    for (const std::size_t piece : {std::size_t{1}, std::size_t{7}, std::size_t{100}, std::size_t{4096}})
    {
        INFO("piece " << piece);
        Synth whole(RATE);
        Synth parts(RATE);
        for (Synth* synth : {&whole, &parts})
        {
            synth->play_music(Tune::GAME);
            synth->play(Effect::CRASH);
        }
        const auto expected = render(whole, 20000);
        std::vector<std::int16_t> actual;
        while (actual.size() < expected.size())
        {
            const auto chunk = render(parts, std::min(piece, expected.size() - actual.size()));
            actual.insert(actual.end(), chunk.begin(), chunk.end());
        }
        CHECK(actual == expected);
    }
}

TEST_CASE("A square wave jumps and has the pitch of its note", "[synth]")
{
    Synth synth(RATE);
    synth.play(Effect::SELECT); // note 84, 1046.5 hertz, for 30 ms
    const auto samples = render(synth, samples_of(Effect::SELECT, RATE));
    // About 31 periods, so about 62 sign changes.
    const std::size_t changes = sign_changes(samples, 0, samples.size());
    CHECK(changes >= 55);
    CHECK(changes <= 70);
    CHECK(largest_step(samples, 0, samples.size()) > 8000);
}

TEST_CASE("A triangle wave is smooth and a noise wave is not a tone", "[synth]")
{
    Synth synth(RATE);
    synth.play(Effect::CRASH); // 140 ms of triangle sliding down, then 300 ms of noise
    const std::size_t triangle_end = 140 * RATE / 1000;
    const auto samples = render(synth, samples_of(Effect::CRASH, RATE));
    // A triangle changes by a small step at a time.
    CHECK(largest_step(samples, 0, triangle_end - 10) < 4000);
    // Noise has both signs, changes often, and jumps.
    const std::size_t noise_changes = sign_changes(samples, triangle_end + 50, samples.size() - 50);
    CHECK(noise_changes > 30);
    CHECK(largest_step(samples, triangle_end + 50, samples.size() - 200) > 8000);
}

TEST_CASE("A slide goes from the first note to the second", "[synth]")
{
    Synth synth(RATE);
    synth.play(Effect::EAT); // note 72 to 79 over 50 ms, then 79 to 91 over 70 ms: rising
    const auto samples = render(synth, samples_of(Effect::EAT, RATE));
    const std::size_t half = samples.size() / 2;
    const std::size_t early = sign_changes(samples, 0, half);
    const std::size_t late = sign_changes(samples, half, samples.size());
    // Early sign changes per sample are fewer than late: the pitch rose.
    CHECK(early * 1 < late * 1);
    CHECK(early > 10);
}

TEST_CASE("A second effect plays over the first", "[synth]")
{
    Synth one(RATE);
    one.play(Effect::EAT);
    const auto alone = render(one, 1500);

    Synth two(RATE);
    two.play(Effect::EAT);
    two.play(Effect::BONUS);
    const auto together = render(two, 1500);
    CHECK(alone != together);
}

TEST_CASE("A fifth effect takes the voice of the one that began first", "[synth]")
{
    Synth synth(RATE);
    for (int i = 0; i < 4; ++i)
    {
        synth.play(Effect::NEW_BEST);
        render(synth, 100);
    }
    synth.play(Effect::SELECT);
    // The first NEW_BEST was cut off; the other three go on after the short effect ends.
    render(synth, 700);
    CHECK_FALSE(synth.effects_idle());
    render(synth, samples_of(Effect::NEW_BEST, RATE));
    CHECK(synth.effects_idle());
}

TEST_CASE("When every voice is busy the one that began longest ago is the one taken, wherever it is", "[synth]")
{
    Synth synth(RATE);
    // The first voice plays a short effect and is free again when the others are still going.
    synth.play(Effect::SELECT);
    for (int i = 0; i < 3; ++i)
    {
        synth.play(Effect::NEW_BEST);
    }
    render(synth, samples_of(Effect::SELECT, RATE) + 10);
    // It takes the free voice, which now began last. The oldest is the second.
    synth.play(Effect::SELECT);
    synth.play(Effect::EAT);
    CHECK_FALSE(synth.effects_idle());
    render(synth, samples_of(Effect::NEW_BEST, RATE) + 100);
    CHECK(synth.effects_idle());
}

TEST_CASE("An effect that is not one of the effects does nothing", "[synth]")
{
    Synth synth(RATE);
    synth.play(static_cast<Effect>(200));
    CHECK(synth.effects_idle());
    CHECK(silent(render(synth, 100)));
}

TEST_CASE("Music plays until it is stopped and loops without a seam", "[synth]")
{
    Synth synth(RATE);
    synth.play_music(Tune::TITLE);
    CHECK(synth.music_playing());
    const TuneData data = tune_data(Tune::TITLE);
    std::size_t steps = 0;
    for (const TuneNote& note : data.channels[0].notes)
    {
        steps += note.steps;
    }
    const std::size_t loop = steps * (static_cast<std::size_t>(data.step_ms) * RATE / 1000);
    const auto first = render(synth, loop);
    const auto second = render(synth, loop);
    CHECK_FALSE(silent(first));
    // The tune comes round again exactly.
    CHECK(first == second);

    synth.stop_music();
    CHECK_FALSE(synth.music_playing());
    CHECK(silent(render(synth, 1000)));
}

TEST_CASE("A new tune replaces the tune that was playing and starts from its beginning", "[synth]")
{
    Synth reference(RATE);
    reference.play_music(Tune::GAME);
    const auto expected = render(reference, 5000);

    Synth synth(RATE);
    synth.play_music(Tune::TITLE);
    render(synth, 3000);
    synth.play_music(Tune::GAME);
    CHECK(render(synth, 5000) == expected);
}

TEST_CASE("A tune that is not one of the tunes plays nothing", "[synth]")
{
    Synth synth(RATE);
    synth.play_music(static_cast<Tune>(200));
    CHECK_FALSE(synth.music_playing());
    CHECK(silent(render(synth, 100)));
}

TEST_CASE("Effects sound over the music, and stopping everything is silence", "[synth]")
{
    Synth music_only(RATE);
    music_only.play_music(Tune::GAME);
    const auto without = render(music_only, 3000);

    Synth synth(RATE);
    synth.play_music(Tune::GAME);
    synth.play(Effect::EAT);
    const auto with = render(synth, 3000);
    CHECK(with != without);

    synth.stop_all();
    CHECK(synth.effects_idle());
    CHECK_FALSE(synth.music_playing());
    CHECK(silent(render(synth, 500)));
}

TEST_CASE("Every voice at once stays inside 16 bits", "[synth]")
{
    Synth synth(RATE);
    synth.play_music(Tune::GAME);
    for (const Effect effect : {Effect::CRASH, Effect::NEW_BEST, Effect::BONUS, Effect::START})
    {
        synth.play(effect);
    }
    const auto samples = render(synth, 30000);
    CHECK(std::ranges::all_of(samples, [](std::int16_t sample) { return sample > -32768; }));
    // It is loud but does not sit against the limit all the time.
    const auto loud = std::ranges::count_if(samples, [](std::int16_t sample) { return std::abs(sample) > 30000; });
    CHECK(loud < static_cast<std::ptrdiff_t>(samples.size() / 20));
    CHECK(std::ranges::any_of(samples, [](std::int16_t sample) { return std::abs(sample) > 8000; }));
}

TEST_CASE("The samples are the same on every platform", "[synth][golden]")
{
    // Whole-number arithmetic throughout, so these hashes are the proof. They were produced by this library, at 22,050
    // hertz: the sound of each effect and a hundred samples of silence after it, and three seconds of each tune.
    constexpr std::array<std::uint64_t, EFFECT_COUNT> EFFECT_HASHES = {
        10534880715216641500ULL, 8304761399733524545ULL, 17633963871920230050ULL, 18040924955535476851ULL,
        1429221139020810590ULL,  9459974721539768454ULL, 17489992650220147519ULL, 6460977384834216183ULL,
        14120708273857243573ULL, 2144403250430088201ULL};
    for (std::size_t i = 0; i < EFFECTS.size(); ++i)
    {
        INFO("effect " << i);
        Synth synth(RATE);
        synth.play(EFFECTS[i]);
        CHECK(hash_of(render(synth, samples_of(EFFECTS[i], RATE) + 100)) == EFFECT_HASHES[i]);
    }

    Synth title(RATE);
    title.play_music(Tune::TITLE);
    CHECK(hash_of(render(title, RATE * 3)) == 13650700643963960529ULL);
    Synth game(RATE);
    game.play_music(Tune::GAME);
    CHECK(hash_of(render(game, RATE * 3)) == 17348095409204678865ULL);
}
