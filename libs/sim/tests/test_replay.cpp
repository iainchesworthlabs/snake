// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/sim/autoplayer.hpp"
#include "snake/sim/random.hpp"
#include "snake/sim/replay.hpp"
#include "snake/sim/snapshot.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

using namespace snake::sim;

namespace
{

constexpr Direction ALL[] = {Direction::UP, Direction::DOWN, Direction::LEFT, Direction::RIGHT};

std::string hex(std::span<const std::uint8_t> bytes)
{
    constexpr std::string_view DIGITS = "0123456789abcdef";
    std::string text;
    for (const std::uint8_t byte : bytes)
    {
        text += DIGITS[byte >> 4U];
        text += DIGITS[byte & 0xFU];
    }
    return text;
}

std::vector<std::uint8_t> encoded(const Replay& replay)
{
    std::vector<std::uint8_t> bytes(MAX_ENCODED_REPLAY_SIZE);
    const std::size_t size = encode_replay(replay, bytes);
    REQUIRE(size > 0);
    bytes.resize(size);
    return bytes;
}

/// True when the snake can step from its head in `direction` without hitting a wall or its body. The tail cell
/// counts as free, as the rules have it.
bool safe_step(const Simulation& state, Direction direction)
{
    int x = state.head().x;
    int y = state.head().y;
    x += direction == Direction::LEFT ? -1 : (direction == Direction::RIGHT ? 1 : 0);
    y += direction == Direction::UP ? -1 : (direction == Direction::DOWN ? 1 : 0);
    if (state.edge_rule() == EdgeRule::WRAP)
    {
        x = (x + BOARD_COLUMNS) % BOARD_COLUMNS;
        y = (y + BOARD_ROWS) % BOARD_ROWS;
    }
    else if (x < 0 || y < 0 || x >= BOARD_COLUMNS || y >= BOARD_ROWS)
    {
        return false;
    }
    for (std::size_t part = 0; part + 1 < state.length(); ++part)
    {
        if (state.snake()[part] == Cell{static_cast<std::uint8_t>(x), static_cast<std::uint8_t>(y)})
        {
            return false;
        }
    }
    return true;
}

/// A player of the game that is not clever: it heads for the food one axis at a time, takes a step that does not
/// crash when it can, and turns at random now and then. It only calls the public functions of Recording, as a
/// person's input would.
void play_a_while(Recording& game, std::uint64_t player_seed, int max_ticks)
{
    Random choice(player_seed);
    REQUIRE(game.start());
    for (int tick = 0; tick < max_ticks && game.session().state() == GameState::RUNNING; ++tick)
    {
        const Simulation& state = game.session().simulation();
        const Cell head = state.head();
        const Cell food = state.food();
        const Direction random_turn = ALL[choice.below(4)];
        if (choice.below(16) == 0 && safe_step(state, random_turn))
        {
            static_cast<void>(game.turn(random_turn));
        }
        else
        {
            const Direction horizontal = head.x < food.x ? Direction::RIGHT : Direction::LEFT;
            const Direction vertical = head.y < food.y ? Direction::DOWN : Direction::UP;
            const bool horizontal_first = choice.below(2) == 0;
            const Direction wanted[] = {horizontal_first ? horizontal : vertical,
                                        horizontal_first ? vertical : horizontal,
                                        state.heading(),
                                        Direction::UP,
                                        Direction::DOWN,
                                        Direction::LEFT,
                                        Direction::RIGHT};
            for (const Direction direction : wanted)
            {
                if (direction != opposite(state.heading()) && safe_step(state, direction))
                {
                    static_cast<void>(game.turn(direction));
                    break;
                }
            }
        }
        static_cast<void>(game.tick());
    }
}

Replay record(Mode mode, std::uint64_t seed, std::uint64_t player_seed, int max_ticks)
{
    Recording game(mode, seed);
    play_a_while(game, player_seed, max_ticks);
    const std::optional<Replay> replay = game.finish();
    REQUIRE(replay.has_value());
    return *replay;
}

} // namespace

TEST_CASE("The input log keeps turns in order and refuses ones that go back", "[input_log]")
{
    InputLog log;
    CHECK(log.size() == 0);
    CHECK(log.record(0, Direction::UP));
    CHECK(log.record(3, Direction::LEFT));
    CHECK(log.record(3, Direction::DOWN));
    CHECK_FALSE(log.record(2, Direction::RIGHT));
    REQUIRE(log.size() == 3);
    CHECK(log.events()[0] == InputEvent{.tick = 0, .direction = Direction::UP});
    CHECK(log.events()[2] == InputEvent{.tick = 3, .direction = Direction::DOWN});
}

TEST_CASE("The input log has room for its capacity and no more", "[input_log]")
{
    InputLog log;
    for (std::size_t turn = 0; turn < InputLog::CAPACITY; ++turn)
    {
        REQUIRE_FALSE(log.full());
        REQUIRE(log.record(static_cast<std::uint32_t>(turn), Direction::UP));
    }
    CHECK(log.full());
    CHECK_FALSE(log.record(100000, Direction::UP));
    CHECK(log.size() == InputLog::CAPACITY);
}

TEST_CASE("Input logs are equal when they hold the same turns", "[input_log]")
{
    InputLog first;
    InputLog second;
    CHECK(first == second);
    REQUIRE(first.record(1, Direction::UP));
    CHECK_FALSE(first == second);
    REQUIRE(second.record(1, Direction::UP));
    CHECK(first == second);
    REQUIRE(first.record(2, Direction::UP));
    REQUIRE(second.record(2, Direction::DOWN));
    CHECK_FALSE(first == second);
}

TEST_CASE("A recording writes down the turns the game accepts, with the tick they were made on", "[recording]")
{
    Recording game(Mode::CLASSIC, 7);
    CHECK_FALSE(game.turn(Direction::UP)); // not started: the game takes no turns
    REQUIRE(game.start());
    CHECK(game.turn(Direction::UP));
    CHECK_FALSE(game.turn(Direction::UP));   // a repeat is dropped
    CHECK_FALSE(game.turn(Direction::DOWN)); // a reversal is dropped
    static_cast<void>(game.tick());
    REQUIRE(game.pause());
    CHECK_FALSE(game.turn(Direction::LEFT)); // dropped while paused
    REQUIRE(game.resume());
    CHECK(game.turn(Direction::LEFT));
    static_cast<void>(game.tick());

    const std::optional<Replay> replay = game.finish();
    REQUIRE(replay.has_value());
    REQUIRE(replay->inputs.size() == 2);
    CHECK(replay->inputs.events()[0] == InputEvent{.tick = 0, .direction = Direction::UP});
    CHECK(replay->inputs.events()[1] == InputEvent{.tick = 1, .direction = Direction::LEFT});
    CHECK(replay->mode == Mode::CLASSIC);
    CHECK(replay->seed == 7);
    CHECK(replay->ticks == 2);
}

TEST_CASE("A recording that ran out of room in the log makes no replay", "[recording]")
{
    // Easy wraps, so a staircase of turns can go on for a long time without crashing.
    Recording game(Mode::EASY, 3);
    REQUIRE(game.start());
    for (std::size_t turn = 0; turn < InputLog::CAPACITY && game.session().state() == GameState::RUNNING; ++turn)
    {
        static_cast<void>(game.turn(turn % 2 == 0 ? Direction::UP : Direction::RIGHT));
        static_cast<void>(game.tick());
    }
    REQUIRE(game.session().state() == GameState::RUNNING);
    REQUIRE(game.complete());
    CHECK(game.finish().has_value());

    // One more accepted turn does not fit. The game still takes it.
    const bool taken = game.turn(Direction::DOWN) || game.turn(Direction::LEFT);
    CHECK(taken);
    CHECK_FALSE(game.complete());
    CHECK_FALSE(game.finish().has_value());
}

TEST_CASE("A recorded game plays back to the same ticks, score and hash", "[replay]")
{
    const Replay replay = record(Mode::EASY, 5, 99, 600);
    REQUIRE(replay.ticks > 100);
    const ReplayOutcome outcome = play(replay);
    CHECK(outcome.ticks == replay.ticks);
    CHECK(outcome.score == replay.score);
    CHECK(outcome.hash == replay.final_hash);
    CHECK(verify(replay));
}

TEST_CASE("A replay fails to verify when any part of it is changed", "[replay]")
{
    const Replay replay = record(Mode::EASY, 5, 99, 600);
    REQUIRE(verify(replay));

    Replay other_seed = replay;
    other_seed.seed += 1;
    CHECK_FALSE(verify(other_seed));

    Replay other_mode = replay;
    other_mode.mode = Mode::CLASSIC;
    CHECK_FALSE(verify(other_mode));

    Replay other_score = replay;
    other_score.score += 1;
    CHECK_FALSE(verify(other_score));

    Replay other_hash = replay;
    other_hash.final_hash ^= 1;
    CHECK_FALSE(verify(other_hash));

    Replay longer = replay;
    longer.ticks += 1;
    CHECK_FALSE(verify(longer));

    Replay fewer_turns;
    fewer_turns.mode = replay.mode;
    fewer_turns.seed = replay.seed;
    fewer_turns.ticks = replay.ticks;
    fewer_turns.score = replay.score;
    fewer_turns.final_hash = replay.final_hash;
    for (std::size_t index = 1; index < replay.inputs.size(); ++index)
    {
        REQUIRE(fewer_turns.inputs.record(replay.inputs.events()[index].tick, replay.inputs.events()[index].direction));
    }
    CHECK_FALSE(verify(fewer_turns));
}

TEST_CASE("A replay of a game that ended plays to the end and stops", "[replay]")
{
    const Replay replay = record(Mode::CLASSIC, 8, 12, 5000);
    const ReplayOutcome outcome = play(replay);
    REQUIRE(outcome.state == GameState::GAME_OVER);
    CHECK(verify(replay));

    // Asked for more ticks than the game lasted, the replay stops where the game ended and does not verify.
    Replay longer = replay;
    longer.ticks += 50;
    const ReplayOutcome stopped = play(longer);
    CHECK(stopped.ticks == replay.ticks);
    CHECK_FALSE(verify(longer));
}

TEST_CASE("Turns made after the last tick are part of the replay", "[replay]")
{
    Recording game(Mode::CLASSIC, 4);
    REQUIRE(game.start());
    static_cast<void>(game.tick());
    REQUIRE(game.turn(Direction::UP)); // waits in the buffer: no tick follows
    const std::optional<Replay> replay = game.finish();
    REQUIRE(replay.has_value());
    CHECK(replay->ticks == 1);
    REQUIRE(replay->inputs.size() == 1);
    CHECK(replay->inputs.events()[0].tick == 1);
    CHECK(verify(*replay));

    Replay without_turn = *replay;
    without_turn.inputs = InputLog{};
    CHECK_FALSE(verify(without_turn)); // the waiting turn is in the hash
}

TEST_CASE("The replay file has the layout the format describes", "[replay][format]")
{
    // Worked out apart from the library from the layout in replay.hpp.
    Replay replay;
    replay.mode = Mode::EASY;
    replay.seed = 0x0102030405060708ULL;
    replay.ticks = 1000;
    replay.score = 70;
    replay.final_hash = 0x1122334455667788ULL;
    REQUIRE(replay.inputs.record(0, Direction::UP));
    REQUIRE(replay.inputs.record(3, Direction::RIGHT));
    REQUIRE(replay.inputs.record(3, Direction::DOWN));
    REQUIRE(replay.inputs.record(300, Direction::LEFT));
    REQUIRE(replay.inputs.record(1000, Direction::UP));
    const std::vector<std::uint8_t> bytes = encoded(replay);
    CHECK(hex(bytes) == "534e4b5201000807060504030201e803000046000000887766554433221105000000000003030001a90202bc0500");
    CHECK(decode_replay(bytes) == replay);
}

TEST_CASE("Decoding what was encoded gives the same replay back", "[replay][format]")
{
    for (const Mode mode : {Mode::EASY, Mode::CLASSIC})
    {
        const Replay replay = record(mode, 21, 4, 2000);
        const std::optional<Replay> decoded = decode_replay(encoded(replay));
        REQUIRE(decoded.has_value());
        CHECK(*decoded == replay);
        CHECK(verify(*decoded));
    }
}

TEST_CASE("A replay with no turns encodes and decodes", "[replay][format]")
{
    const Replay replay = record(Mode::CLASSIC, 2, 1, 0);
    CHECK(replay.inputs.size() == 0);
    CHECK(decode_replay(encoded(replay)) == replay);
}

TEST_CASE("Encoding into a buffer that is too small writes nothing it can be mistaken for", "[replay][format]")
{
    const Replay replay = record(Mode::EASY, 5, 99, 100);
    const std::size_t size = encoded(replay).size();
    std::vector<std::uint8_t> bytes(size);
    CHECK(encode_replay(replay, bytes) == size);
    bytes.resize(size - 1);
    CHECK(encode_replay(replay, bytes) == 0);
    CHECK(encode_replay(replay, std::span<std::uint8_t>{}) == 0);
}

TEST_CASE("The largest replay fits the largest size", "[replay][format]")
{
    Replay replay;
    replay.ticks = 0xFFFFFFFFU;
    for (std::size_t turn = 0; turn < InputLog::CAPACITY; ++turn)
    {
        REQUIRE(replay.inputs.record(static_cast<std::uint32_t>(turn) * 500000U, Direction::UP));
    }
    std::vector<std::uint8_t> bytes(MAX_ENCODED_REPLAY_SIZE);
    const std::size_t size = encode_replay(replay, bytes);
    CHECK(size > 0);
    CHECK(size <= MAX_ENCODED_REPLAY_SIZE);
    bytes.resize(size);
    CHECK(decode_replay(bytes) == replay);
}

TEST_CASE("Bytes that are cut short are refused at every length", "[replay][format]")
{
    const std::vector<std::uint8_t> bytes = encoded(record(Mode::EASY, 5, 99, 100));
    for (std::size_t size = 0; size < bytes.size(); ++size)
    {
        CHECK_FALSE(decode_replay(std::span<const std::uint8_t>(bytes.data(), size)).has_value());
    }
    CHECK(decode_replay(bytes).has_value());
}

TEST_CASE("Bytes with something left over are refused", "[replay][format]")
{
    std::vector<std::uint8_t> bytes = encoded(record(Mode::EASY, 5, 99, 100));
    bytes.push_back(0);
    CHECK_FALSE(decode_replay(bytes).has_value());
}

TEST_CASE("Bytes with a wrong marker, version, mode or direction are refused", "[replay][format]")
{
    Replay replay;
    replay.ticks = 10;
    REQUIRE(replay.inputs.record(2, Direction::LEFT));
    const std::vector<std::uint8_t> good = encoded(replay);
    REQUIRE(decode_replay(good).has_value());
    const auto refused_with = [&](std::size_t position, std::uint8_t value)
    {
        std::vector<std::uint8_t> bytes = good;
        REQUIRE(position < bytes.size());
        bytes[position] = value;
        return !decode_replay(bytes).has_value();
    };
    CHECK(refused_with(0, 'X'));
    CHECK(refused_with(3, 'S'));
    CHECK(refused_with(4, 2));               // a version this code does not know
    CHECK(refused_with(5, 4));               // a mode that does not exist
    CHECK_FALSE(refused_with(5, 3));         // Hot potato is the last mode
    CHECK(refused_with(good.size() - 1, 5)); // an input that is neither a direction (0 to 3) nor a shed (4)
    CHECK_FALSE(refused_with(good.size() - 1, 4));
    CHECK_FALSE(refused_with(good.size() - 1, 3));
}

TEST_CASE("Bytes that claim more turns than a log holds are refused", "[replay][format]")
{
    std::vector<std::uint8_t> bytes = encoded(Replay{});
    // The turn count is the 4 bytes before the end of a replay with no turns.
    const std::size_t count_at = bytes.size() - 4;
    bytes[count_at] = 0x01;
    bytes[count_at + 1] = 0x20; // 8193
    CHECK_FALSE(decode_replay(bytes).has_value());
}

TEST_CASE("A turn number that is badly written or out of range is refused", "[replay][format]")
{
    // A replay of 10 ticks with one turn, then its single turn written by hand.
    Replay replay;
    replay.ticks = 10;
    REQUIRE(replay.inputs.record(2, Direction::LEFT));
    const std::vector<std::uint8_t> good = encoded(replay);
    const std::size_t turn_at = good.size() - 2; // the number, one byte, and then the direction
    const auto with_turn = [&](std::vector<std::uint8_t> number, std::uint8_t direction)
    {
        std::vector<std::uint8_t> bytes(good.begin(), good.begin() + static_cast<std::ptrdiff_t>(turn_at));
        bytes.insert(bytes.end(), number.begin(), number.end());
        bytes.push_back(direction);
        return decode_replay(bytes);
    };
    CHECK(with_turn({10}, 0).has_value());                                       // the last tick is fine
    CHECK_FALSE(with_turn({11}, 0).has_value());                                 // after the last tick
    CHECK(with_turn({0x82, 0x00}, 0).has_value());                               // 2 written in two groups
    CHECK_FALSE(with_turn({0x80, 0x80, 0x80, 0x80, 0x80, 0x00}, 0).has_value()); // six groups
    CHECK_FALSE(with_turn({0xFF, 0xFF, 0xFF, 0xFF, 0x1F}, 0).has_value());       // above 32 bits
    CHECK_FALSE(with_turn({0x80}, 0).has_value());                               // cut short in the middle of a number
}

TEST_CASE("Turn numbers add up from the gaps between turns", "[replay][format]")
{
    Replay replay;
    replay.ticks = 0xFFFFFFFFU;
    REQUIRE(replay.inputs.record(0xFFFFFFFFU, Direction::UP));
    const std::vector<std::uint8_t> good = encoded(replay);
    REQUIRE(decode_replay(good) == replay);

    // Two gaps that together pass 32 bits.
    Replay two;
    two.ticks = 0xFFFFFFFFU;
    REQUIRE(two.inputs.record(0xFFFFFFFFU, Direction::UP));
    REQUIRE(two.inputs.record(0xFFFFFFFFU, Direction::LEFT));
    std::vector<std::uint8_t> bytes = encoded(two);
    // Change the second gap from 0 to 1.
    bytes[bytes.size() - 2] = 1;
    CHECK_FALSE(decode_replay(bytes).has_value());
}

TEST_CASE("Golden replays: recorded games give the same result on every platform", "[replay][golden]")
{
    struct Golden
    {
        Mode mode;
        std::uint64_t seed;
        std::uint64_t player_seed;
        int max_ticks;
        std::uint32_t ticks;
        std::uint32_t score;
        std::uint64_t hash;
        std::size_t turns;
    };
    const Golden games[] = {
        {Mode::EASY, 1, 100, 3000, 293, 340, 0xb5561a63b4d5fc99ULL, 183},
        {Mode::EASY, 2, 200, 3000, 638, 590, 0xb0970c14de194e3dULL, 387},
        {Mode::CLASSIC, 3, 300, 3000, 364, 490, 0xda1d2433b542c242ULL, 228},
        {Mode::CLASSIC, 4, 400, 3000, 467, 800, 0xc2b7483294867108ULL, 278},
        {Mode::EASY, 5, 500, 3000, 751, 420, 0xb96071ede2ef9210ULL, 428},
        {Mode::CLASSIC, 6, 600, 3000, 396, 780, 0x75a5b5c9e5c9e417ULL, 237},
    };
    for (const Golden& golden : games)
    {
        INFO("mode " << static_cast<int>(golden.mode) << " seed " << golden.seed);
        const Replay replay = record(golden.mode, golden.seed, golden.player_seed, golden.max_ticks);
        CHECK(replay.ticks == golden.ticks);
        CHECK(replay.score == golden.score);
        CHECK(replay.final_hash == golden.hash);
        CHECK(replay.inputs.size() == golden.turns);
        CHECK(verify(replay));
        const std::optional<Replay> from_file = decode_replay(encoded(replay));
        REQUIRE(from_file.has_value());
        CHECK(verify(*from_file));
    }
}

TEST_CASE("Two replays are equal only when every part is", "[replay]")
{
    const Replay base = record(Mode::EASY, 5, 99, 100);
    CHECK(base == record(Mode::EASY, 5, 99, 100));
    const auto differs = [&](auto change)
    {
        Replay other = base;
        change(other);
        return !(other == base);
    };
    CHECK(differs([](Replay& r) { r.mode = Mode::CLASSIC; }));
    CHECK(differs([](Replay& r) { r.seed += 1; }));
    CHECK(differs([](Replay& r) { r.ticks += 1; }));
    CHECK(differs([](Replay& r) { r.score += 1; }));
    CHECK(differs([](Replay& r) { r.final_hash += 1; }));
    CHECK(differs([](Replay& r) { static_cast<void>(r.inputs.record(100000, Direction::UP)); }));
}

TEST_CASE("Two input events are equal only when the tick and the direction are", "[input_log]")
{
    const InputEvent event{.tick = 4, .direction = Direction::LEFT};
    CHECK(event == InputEvent{.tick = 4, .direction = Direction::LEFT});
    CHECK_FALSE(event == InputEvent{.tick = 5, .direction = Direction::LEFT});
    CHECK_FALSE(event == InputEvent{.tick = 4, .direction = Direction::UP});
}

namespace
{

/// A game of `mode` played by the computer player for up to `ticks` ticks, shedding the tail as soon as it can.
Replay recorded_by_autoplay(Mode mode, std::uint64_t seed, std::uint32_t ticks, bool shed)
{
    Recording recording(mode, seed);
    REQUIRE(recording.start());
    for (std::uint32_t tick = 0; tick < ticks && recording.session().state() == GameState::RUNNING; ++tick)
    {
        static_cast<void>(recording.turn(autoplay(recording.session().simulation())));
        if (shed)
        {
            static_cast<void>(recording.shed());
        }
        static_cast<void>(recording.tick());
    }
    const std::optional<Replay> replay = recording.finish();
    REQUIRE(replay.has_value());
    return *replay;
}

} // namespace

TEST_CASE("Games of Portals and Hot potato replay to the same end, and so do their saved bytes", "[replay][modes]")
{
    for (const Mode mode : {Mode::PORTALS, Mode::HOT_POTATO})
    {
        for (std::uint64_t seed = 1; seed <= 5; ++seed)
        {
            const Replay replay = recorded_by_autoplay(mode, seed, 1500, true);
            REQUIRE(replay.ticks > 100);
            CHECK(verify(replay));
            const std::optional<Replay> back = decode_replay(encoded(replay));
            REQUIRE(back.has_value());
            CHECK(*back == replay);
            CHECK(verify(*back));
        }
    }
}

TEST_CASE("A shed is part of a replay: left out, the game goes another way", "[replay][shed]")
{
    // The computer player collects bomb foods, and with sheds the game goes much further than without.
    bool some_shed = false;
    for (std::uint64_t seed = 1; seed <= 10 && !some_shed; ++seed)
    {
        const Replay replay = recorded_by_autoplay(Mode::HOT_POTATO, seed, 2500, true);
        for (const InputEvent& event : replay.inputs.events())
        {
            some_shed |= event.shed;
        }
        if (!some_shed)
        {
            continue;
        }
        REQUIRE(verify(replay));
        Replay without = replay;
        InputLog log;
        for (const InputEvent& event : replay.inputs.events())
        {
            if (!event.shed)
            {
                REQUIRE(log.record(event.tick, event.direction));
            }
        }
        without.inputs = log;
        CHECK_FALSE(verify(without));
    }
    CHECK(some_shed);
}

TEST_CASE("A shed is written as the byte 4 and read back", "[replay][shed][format]")
{
    Replay replay;
    replay.mode = Mode::HOT_POTATO;
    replay.ticks = 20;
    REQUIRE(replay.inputs.record(3, Direction::UP));
    REQUIRE(replay.inputs.record_shed(7));
    REQUIRE(replay.inputs.record(7, Direction::LEFT));
    const std::vector<std::uint8_t> bytes = encoded(replay);
    // The last six bytes are the three inputs: the ticks since the one before, and the byte of each.
    REQUIRE(bytes.size() >= 6);
    CHECK(bytes[bytes.size() - 6] == 3); // 3 ticks after the start: up
    CHECK(bytes[bytes.size() - 5] == 0);
    CHECK(bytes[bytes.size() - 4] == 4); // 4 ticks later: a shed
    CHECK(bytes[bytes.size() - 3] == 4);
    CHECK(bytes[bytes.size() - 2] == 0); // and a turn left on the same tick
    CHECK(bytes[bytes.size() - 1] == 2);
    const std::optional<Replay> back = decode_replay(bytes);
    REQUIRE(back.has_value());
    CHECK(*back == replay);
    CHECK(back->inputs.events()[1].shed);
}

TEST_CASE("A turn or a shed that the game does not take is not written down", "[replay][shed]")
{
    Recording recording(Mode::HOT_POTATO, 5);
    CHECK_FALSE(recording.shed()); // not started
    REQUIRE(recording.start());
    CHECK_FALSE(recording.shed()); // no bomb
    const std::optional<Replay> replay = recording.finish();
    REQUIRE(replay.has_value());
    CHECK(replay->inputs.size() == 0);
}
