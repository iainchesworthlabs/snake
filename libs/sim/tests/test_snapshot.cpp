// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/sim/snapshot.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using namespace snake::sim;

namespace
{

constexpr Direction ALL[] = {Direction::UP, Direction::DOWN, Direction::LEFT, Direction::RIGHT};

std::vector<std::uint8_t> encoded(const Snapshot& snapshot)
{
    std::array<std::uint8_t, MAX_ENCODED_SNAPSHOT_SIZE> bytes{};
    const std::size_t size = encode(snapshot, bytes);
    REQUIRE(size > 0);
    return std::vector<std::uint8_t>(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(size));
}

std::string_view hex_digits()
{
    return "0123456789abcdef";
}

std::string hex(const std::vector<std::uint8_t>& bytes)
{
    std::string text;
    for (const std::uint8_t byte : bytes)
    {
        text += hex_digits()[byte >> 4U];
        text += hex_digits()[byte & 0xFU];
    }
    return text;
}

/// The game of the second pinned state: a position with a bonus item and two turns waiting.
Simulation busy_game()
{
    const std::vector<Cell> snake{{5, 5}, {4, 5}, {3, 5}};
    const std::vector<Direction> pending{Direction::UP, Direction::LEFT};
    const std::optional<Simulation> game = Simulation::from_setup(
        {.edges = EdgeRule::WRAP,
         .heading = Direction::RIGHT,
         .snake = snake,
         .food = {6, 5},
         .seed = 11,
         .scoring = {.score = 60, .foods_eaten = 3, .normal_since_bonus = 3, .combo = 3, .ticks_since_food = 2},
         .bonus = Simulation::Bonus{.cell = {10, 12}, .ticks_left = 9},
         .ticks = 17,
         .pending = pending});
    REQUIRE(game.has_value());
    return *game;
}

/// A game played on with random turns, as far as `ticks` ticks or until it ends.
void play(Simulation& game, Random& inputs, int ticks)
{
    for (int tick = 0; tick < ticks && game.running(); ++tick)
    {
        if (inputs.below(3) == 0)
        {
            static_cast<void>(game.turn(ALL[inputs.below(4)]));
        }
        static_cast<void>(game.tick());
    }
}

} // namespace

TEST_CASE("The tick counter counts ticks played, including the one that ends the game", "[snapshot][ticks]")
{
    Simulation game(EdgeRule::WALLS, 42);
    CHECK(game.ticks() == 0);
    int played = 0;
    while (game.running())
    {
        static_cast<void>(game.tick());
        ++played;
    }
    CHECK(game.ticks() == static_cast<std::uint32_t>(played));
    CHECK(played == 10); // nine moves to the right wall and the tick that hits it
    static_cast<void>(game.tick());
    CHECK(game.ticks() == 10);
}

TEST_CASE("A snapshot of a new game holds the game's state", "[snapshot]")
{
    const Simulation game(EdgeRule::WALLS, 42);
    const Snapshot snapshot = capture(game);
    CHECK(snapshot.edges == EdgeRule::WALLS);
    CHECK(snapshot.heading == Direction::RIGHT);
    CHECK(snapshot.running);
    CHECK_FALSE(snapshot.won);
    CHECK(snapshot.ticks == 0);
    CHECK(snapshot.length == 3);
    CHECK(snapshot.snake[0] == Cell{10, 10});
    CHECK(snapshot.snake[2] == Cell{8, 10});
    CHECK(snapshot.snake[3] == Cell{0, 0});
    CHECK(snapshot.food == Cell{4, 6});
    CHECK_FALSE(snapshot.bonus.has_value());
    CHECK(snapshot.scoring == Scoring::State{});
    CHECK(snapshot.random == game.random_state());
    CHECK(snapshot.pending_count == 0);
}

TEST_CASE("A snapshot holds the turns that are waiting", "[snapshot]")
{
    const Snapshot snapshot = capture(busy_game());
    CHECK(snapshot.pending_count == 2);
    CHECK(snapshot.pending[0] == Direction::UP);
    CHECK(snapshot.pending[1] == Direction::LEFT);
    REQUIRE(snapshot.bonus.has_value());
    CHECK(snapshot.bonus->cell == Cell{10, 12});
    CHECK(snapshot.ticks == 17);
}

TEST_CASE("A restored game plays on exactly as the original does", "[snapshot][restore]")
{
    for (const EdgeRule edges : {EdgeRule::WALLS, EdgeRule::WRAP})
    {
        for (std::uint64_t seed = 1; seed <= 6; ++seed)
        {
            Simulation original(edges, seed);
            Random inputs(seed * 77);
            play(original, inputs, 150);

            std::optional<Simulation> restored = restore(capture(original));
            REQUIRE(restored.has_value());
            CHECK(capture(*restored) == capture(original));
            CHECK(state_hash(*restored) == state_hash(original));

            // Both get the same turns from here on and stay equal, whether or not the game has ended.
            Random more_inputs(seed * 991);
            for (int tick = 0; tick < 400; ++tick)
            {
                const Direction turn = ALL[more_inputs.below(4)];
                REQUIRE(original.turn(turn) == restored->turn(turn));
                REQUIRE(original.tick() == restored->tick());
                REQUIRE(state_hash(*restored) == state_hash(original));
            }
        }
    }
}

TEST_CASE("A game that has ended or been won restores as it was", "[snapshot][restore]")
{
    Simulation lost(EdgeRule::WALLS, 5);
    while (lost.running())
    {
        static_cast<void>(lost.tick());
    }
    const std::optional<Simulation> restored = restore(capture(lost));
    REQUIRE(restored.has_value());
    CHECK_FALSE(restored->running());
    CHECK_FALSE(restored->won());
    CHECK(capture(*restored) == capture(lost));

    Snapshot won = capture(Simulation(EdgeRule::WALLS, 5));
    won.running = false;
    won.won = true;
    CHECK_FALSE(restore(won).has_value()); // not a full board
}

TEST_CASE("A game won by filling the board restores as it was", "[snapshot][restore]")
{
    std::vector<Cell> path;
    for (std::uint8_t row = 0; row < BOARD_ROWS; ++row)
    {
        for (std::uint8_t column = 0; column < BOARD_COLUMNS; ++column)
        {
            path.push_back(Cell{static_cast<std::uint8_t>(row % 2 == 0 ? column : BOARD_COLUMNS - 1 - column), row});
        }
    }
    path.resize(399);
    const std::vector<Cell> head_first(path.rbegin(), path.rend());
    std::optional<Simulation> game = Simulation::from_setup(
        {.edges = EdgeRule::WALLS, .heading = Direction::LEFT, .snake = head_first, .food = {0, 19}});
    REQUIRE(game.has_value());
    REQUIRE(game->tick() == TickOutcome::WON);

    const std::optional<Simulation> restored = restore(capture(*game));
    REQUIRE(restored.has_value());
    CHECK(restored->won());
    CHECK_FALSE(restored->running());
    CHECK(restored->length() == BOARD_CELLS);
    CHECK(capture(*restored) == capture(*game));
}

TEST_CASE("The encoded bytes of a new game are the ones the format describes", "[snapshot][format]")
{
    // Worked out apart from the library from the layout in snapshot.hpp and the generator's reference algorithm.
    const Simulation game(EdgeRule::WALLS, 42);
    CHECK(hex(encoded(capture(game))) ==
          "010003010000000003000a0a090a080a040600000000000000011505bb6687d8fe472b6d0000000000000000");
    CHECK(state_hash(game) == 0xae44dd584d769bb4ULL);
}

TEST_CASE("The encoded bytes of a game with a bonus item and waiting turns are the ones the format describes",
          "[snapshot][format]")
{
    const Simulation game = busy_game();
    CHECK(hex(encoded(capture(game))) ==
          "0101030511000000030005050405030506050a0c093c0000000300030302859d13e63b756a666d00000000000000020002");
    CHECK(state_hash(game) == 0x91f17a5f61dd5047ULL);
}

TEST_CASE("Decoding what was encoded gives the same snapshot back", "[snapshot][format]")
{
    for (std::uint64_t seed = 1; seed <= 8; ++seed)
    {
        Simulation game(seed % 2 == 0 ? EdgeRule::WALLS : EdgeRule::WRAP, seed);
        Random inputs(seed);
        play(game, inputs, 60 * static_cast<int>(seed));
        const Snapshot snapshot = capture(game);
        const std::vector<std::uint8_t> bytes = encoded(snapshot);
        const std::optional<Snapshot> decoded = decode(bytes);
        REQUIRE(decoded.has_value());
        CHECK(*decoded == snapshot);
    }
    const Snapshot busy = capture(busy_game());
    CHECK(decode(encoded(busy)) == busy);
}

TEST_CASE("A snapshot of a full board fits the largest size", "[snapshot][format]")
{
    Snapshot full;
    full.length = BOARD_CELLS;
    full.running = false;
    full.won = true;
    full.bonus = Simulation::Bonus{};
    full.pending_count = 2;
    std::array<std::uint8_t, MAX_ENCODED_SNAPSHOT_SIZE> bytes{};
    const std::size_t size = encode(full, bytes);
    CHECK(size > 800);
    CHECK(size <= MAX_ENCODED_SNAPSHOT_SIZE);
}

TEST_CASE("Encoding into a buffer that is too small writes nothing it can be mistaken for", "[snapshot][format]")
{
    const Snapshot snapshot = capture(Simulation(EdgeRule::WALLS, 1));
    const std::size_t size = encoded(snapshot).size();
    std::array<std::uint8_t, MAX_ENCODED_SNAPSHOT_SIZE> bytes{};
    CHECK(encode(snapshot, std::span<std::uint8_t>(bytes.data(), size)) == size);
    CHECK(encode(snapshot, std::span<std::uint8_t>(bytes.data(), size - 1)) == 0);
    CHECK(encode(snapshot, std::span<std::uint8_t>{}) == 0);
}

TEST_CASE("A snapshot with a length or a pending count the arrays cannot hold is not encoded", "[snapshot][format]")
{
    std::array<std::uint8_t, MAX_ENCODED_SNAPSHOT_SIZE> bytes{};
    Snapshot long_snake = capture(Simulation(EdgeRule::WALLS, 1));
    long_snake.length = BOARD_CELLS + 1;
    CHECK(encode(long_snake, bytes) == 0);
    Snapshot many_turns = capture(Simulation(EdgeRule::WALLS, 1));
    many_turns.pending_count = 3;
    CHECK(encode(many_turns, bytes) == 0);
    CHECK_FALSE(restore(long_snake).has_value());
    CHECK_FALSE(restore(many_turns).has_value());
}

TEST_CASE("Bytes that are cut short are refused at every length", "[snapshot][format]")
{
    const std::vector<std::uint8_t> bytes = encoded(capture(busy_game()));
    for (std::size_t size = 0; size < bytes.size(); ++size)
    {
        CHECK_FALSE(decode(std::span<const std::uint8_t>(bytes.data(), size)).has_value());
    }
    CHECK(decode(bytes).has_value());
}

TEST_CASE("Bytes with something left over are refused", "[snapshot][format]")
{
    std::vector<std::uint8_t> bytes = encoded(capture(busy_game()));
    bytes.push_back(0);
    CHECK_FALSE(decode(bytes).has_value());
}

TEST_CASE("Bytes with a value no snapshot has are refused", "[snapshot][format]")
{
    const std::vector<std::uint8_t> good = encoded(capture(busy_game()));
    const auto refused_with = [&](std::size_t position, std::uint8_t value)
    {
        std::vector<std::uint8_t> bytes = good;
        REQUIRE(position < bytes.size());
        bytes[position] = value;
        return !decode(bytes).has_value();
    };
    CHECK(refused_with(0, 2));    // a version this code does not know
    CHECK(refused_with(1, 2));    // an edge rule above wrap
    CHECK(refused_with(2, 4));    // a heading above right
    CHECK(refused_with(3, 0x8F)); // a flag that is not defined
    CHECK_FALSE(refused_with(3, 0x07));
    // The last three bytes: the pending count and the two waiting turns.
    CHECK(refused_with(good.size() - 3, 3));
    CHECK(refused_with(good.size() - 1, 4));
    CHECK_FALSE(refused_with(good.size() - 1, 3));

    // A snake longer than the board.
    std::vector<std::uint8_t> bytes = good;
    bytes[8] = 0x91;
    bytes[9] = 0x01;
    CHECK_FALSE(decode(bytes).has_value());
}

TEST_CASE("A snapshot of a game that cannot happen is not restored", "[snapshot][restore]")
{
    const Snapshot good = capture(busy_game());
    REQUIRE(restore(good).has_value());

    Snapshot even_increment = good;
    even_increment.random.increment = 2;
    CHECK_FALSE(restore(even_increment).has_value());

    Snapshot off_board = good;
    off_board.food = Cell{20, 0};
    CHECK_FALSE(restore(off_board).has_value());

    Snapshot reversal = good;
    reversal.pending[0] = Direction::LEFT; // against the heading
    CHECK_FALSE(restore(reversal).has_value());

    Snapshot won_and_running = good;
    won_and_running.won = true;
    CHECK_FALSE(restore(won_and_running).has_value());

    Snapshot odd_scoring = good;
    odd_scoring.scoring.combo = 0;
    CHECK_FALSE(restore(odd_scoring).has_value());
}

TEST_CASE("Two snapshots are equal only when every field is", "[snapshot]")
{
    const Snapshot base = capture(busy_game());
    const auto differs = [&](auto change)
    {
        Snapshot other = base;
        change(other);
        return !(other == base);
    };
    CHECK(base == capture(busy_game()));
    CHECK(differs([](Snapshot& s) { s.edges = EdgeRule::WALLS; }));
    CHECK(differs([](Snapshot& s) { s.heading = Direction::UP; }));
    CHECK(differs([](Snapshot& s) { s.running = false; }));
    CHECK(differs([](Snapshot& s) { s.won = true; }));
    CHECK(differs([](Snapshot& s) { s.ticks += 1; }));
    CHECK(differs([](Snapshot& s) { s.length -= 1; }));
    CHECK(differs([](Snapshot& s) { s.snake[1] = Cell{9, 9}; }));
    CHECK(differs([](Snapshot& s) { s.food = Cell{9, 9}; }));
    CHECK(differs([](Snapshot& s) { s.bonus.reset(); }));
    CHECK(differs([](Snapshot& s) { s.bonus = Simulation::Bonus{.cell = Cell{1, 1}, .ticks_left = 9}; }));
    CHECK(differs([](Snapshot& s) { s.bonus = Simulation::Bonus{.cell = Cell{10, 12}, .ticks_left = 8}; }));
    CHECK(differs([](Snapshot& s) { s.scoring.score += 1; }));
    CHECK(differs([](Snapshot& s) { s.random.state += 1; }));
    CHECK(differs([](Snapshot& s) { s.random.increment += 2; }));
    CHECK(differs([](Snapshot& s) { s.pending_count = 1; }));
    CHECK(differs([](Snapshot& s) { s.pending[1] = Direction::DOWN; }));
}

TEST_CASE("The hash changes with every part of the state", "[snapshot][hash]")
{
    const Snapshot base = capture(busy_game());
    const std::uint64_t reference = state_hash(base);
    const auto hash_with = [&](auto change)
    {
        Snapshot other = base;
        change(other);
        return state_hash(other);
    };
    CHECK(hash_with([](Snapshot& s) { s.edges = EdgeRule::WALLS; }) != reference);
    CHECK(hash_with([](Snapshot& s) { s.heading = Direction::UP; }) != reference);
    CHECK(hash_with([](Snapshot& s) { s.ticks += 1; }) != reference);
    CHECK(hash_with([](Snapshot& s) { s.snake[2] = Cell{3, 6}; }) != reference);
    CHECK(hash_with([](Snapshot& s) { s.food = Cell{7, 5}; }) != reference);
    CHECK(hash_with([](Snapshot& s) { s.bonus = Simulation::Bonus{.cell = Cell{10, 12}, .ticks_left = 8}; }) !=
          reference);
    CHECK(hash_with([](Snapshot& s) { s.scoring.combo = 4; }) != reference);
    CHECK(hash_with([](Snapshot& s) { s.random.state += 1; }) != reference);
    CHECK(hash_with([](Snapshot& s) { s.pending[1] = Direction::DOWN; }) != reference);
    CHECK(hash_with([](Snapshot&) {}) == reference);
}

TEST_CASE("The hash of a game is the hash of its snapshot", "[snapshot][hash]")
{
    const Simulation game = busy_game();
    CHECK(state_hash(game) == state_hash(capture(game)));
}

TEST_CASE("A position with a generator increment that is even is refused", "[snapshot][setup]")
{
    const std::vector<Cell> snake{{5, 5}, {4, 5}};
    CHECK_FALSE(Simulation::from_setup({.heading = Direction::RIGHT,
                                        .snake = snake,
                                        .food = {9, 9},
                                        .random = Random::State{.state = 1, .increment = 4}})
                    .has_value());
    CHECK(Simulation::from_setup({.heading = Direction::RIGHT,
                                  .snake = snake,
                                  .food = {9, 9},
                                  .random = Random::State{.state = 1, .increment = 5}})
              .has_value());
}

TEST_CASE("A position with more waiting turns than the buffer holds is refused", "[snapshot][setup]")
{
    const std::vector<Cell> snake{{5, 5}, {4, 5}};
    const std::vector<Direction> three{Direction::UP, Direction::LEFT, Direction::DOWN};
    CHECK_FALSE(Simulation::from_setup({.heading = Direction::RIGHT, .snake = snake, .food = {9, 9}, .pending = three})
                    .has_value());
}

namespace
{

/// A game of Hot potato with a bomb food on the board.
Simulation game_with_potato()
{
    const std::vector<Cell> snake{{8, 8}, {7, 8}, {6, 8}, {5, 8}, {4, 8}, {3, 8}};
    const std::optional<Simulation> game =
        Simulation::from_setup({.snake = snake,
                                .food = {1, 1},
                                .seed = 3,
                                .ticks = 40,
                                .variant = Variant::HOT_POTATO,
                                .potato = Simulation::Potato{.cell = {12, 12}, .ticks_left = 31},
                                .normal_since_potato = 0});
    REQUIRE(game.has_value());
    return *game;
}

/// A game of Hot potato with a bomb in the snake.
Simulation game_with_lump()
{
    const std::vector<Cell> snake{{8, 8}, {7, 8}, {6, 8}, {5, 8}, {4, 8}, {3, 8}};
    const std::optional<Simulation> game = Simulation::from_setup({.snake = snake,
                                                                   .food = {1, 1},
                                                                   .seed = 3,
                                                                   .ticks = 40,
                                                                   .variant = Variant::HOT_POTATO,
                                                                   .lump = Cell{6, 8},
                                                                   .normal_since_potato = 2});
    REQUIRE(game.has_value());
    return *game;
}

} // namespace

TEST_CASE("A plain game is written as version 1 and a game with a variant as version 2", "[snapshot][format][variant]")
{
    CHECK(encoded(capture(Simulation(EdgeRule::WALLS, 4)))[0] == 1);
    CHECK(encoded(capture(Simulation(EdgeRule::WALLS, 4, Variant::PORTALS)))[0] == 2);
    CHECK(encoded(capture(Simulation(EdgeRule::WALLS, 4, Variant::HOT_POTATO)))[0] == 2);
}

TEST_CASE("The snapshot of a game with a variant holds its pieces and restores them", "[snapshot][variant]")
{
    const Simulation portals(EdgeRule::WALLS, 9, Variant::PORTALS);
    const Snapshot portal_snapshot = capture(portals);
    CHECK(portal_snapshot.variant == Variant::PORTALS);
    CHECK(portal_snapshot.portals == portals.portals());
    const std::optional<Simulation> restored = restore(portal_snapshot);
    REQUIRE(restored.has_value());
    CHECK(capture(*restored) == portal_snapshot);

    const Snapshot potato = capture(game_with_potato());
    CHECK(potato.potato == Simulation::Potato{.cell = {12, 12}, .ticks_left = 31});
    CHECK_FALSE(potato.lump.has_value());
    const Snapshot lump = capture(game_with_lump());
    CHECK(lump.lump == Cell{6, 8});
    CHECK(lump.normal_since_potato == 2);
    CHECK_FALSE(lump.potato.has_value());
    for (const Snapshot& snapshot : {potato, lump})
    {
        const std::optional<Simulation> again = restore(snapshot);
        REQUIRE(again.has_value());
        CHECK(capture(*again) == snapshot);
    }
}

TEST_CASE("Every game with a variant decodes to the snapshot it was encoded from", "[snapshot][format][variant]")
{
    for (const Snapshot& snapshot : {capture(Simulation(EdgeRule::WALLS, 2, Variant::PORTALS)),
                                     capture(Simulation(EdgeRule::WALLS, 2, Variant::HOT_POTATO)),
                                     capture(game_with_potato()), capture(game_with_lump())})
    {
        const std::optional<Snapshot> back = decode(encoded(snapshot));
        REQUIRE(back.has_value());
        CHECK(*back == snapshot);
    }
}

TEST_CASE("A restored game with a variant plays on exactly as the original does", "[snapshot][restore][variant]")
{
    for (const Variant variant : {Variant::PORTALS, Variant::HOT_POTATO})
    {
        Simulation game(EdgeRule::WALLS, 21, variant);
        for (int tick = 0; tick < 300 && game.running(); ++tick)
        {
            static_cast<void>(game.turn(ALL[static_cast<std::size_t>(tick / 7) % 4]));
            static_cast<void>(game.tick());
            if (tick % 50 == 49)
            {
                std::optional<Simulation> copy = restore(capture(game));
                REQUIRE(copy.has_value());
                REQUIRE(state_hash(*copy) == state_hash(game));
                for (int more = 0; more < 20 && game.running(); ++more)
                {
                    static_cast<void>(game.turn(ALL[static_cast<std::size_t>(more) % 4]));
                    static_cast<void>(copy->turn(ALL[static_cast<std::size_t>(more) % 4]));
                    REQUIRE(game.tick() == copy->tick());
                    REQUIRE(state_hash(*copy) == state_hash(game));
                }
                break;
            }
        }
    }
}

TEST_CASE("The hash changes with the pieces of a variant", "[snapshot][hash][variant]")
{
    Snapshot base = capture(Simulation(EdgeRule::WALLS, 9, Variant::PORTALS));
    Snapshot changed = base;
    changed.portals->cells[3].x = static_cast<std::uint8_t>((changed.portals->cells[3].x + 1) % BOARD_COLUMNS);
    CHECK(state_hash(base) != state_hash(changed));

    const Snapshot potato = capture(game_with_potato());
    Snapshot other = potato;
    other.potato->ticks_left = 30;
    CHECK(state_hash(potato) != state_hash(other));
    other = potato;
    other.normal_since_potato = 1;
    CHECK(state_hash(potato) != state_hash(other)); // the count goes in the hash
}

TEST_CASE("A snapshot with pieces that do not belong to its variant is not encoded", "[snapshot][format][variant]")
{
    std::array<std::uint8_t, MAX_ENCODED_SNAPSHOT_SIZE> bytes{};
    Snapshot snapshot = capture(Simulation(EdgeRule::WALLS, 9, Variant::PORTALS));
    snapshot.portals.reset();
    CHECK(encode(snapshot, bytes) == 0);

    snapshot = capture(Simulation(EdgeRule::WALLS, 9));
    snapshot.portals = Simulation::PortalCells{};
    CHECK(encode(snapshot, bytes) == 0);

    snapshot = capture(Simulation(EdgeRule::WALLS, 9, Variant::PORTALS));
    snapshot.lump = Cell{1, 1};
    CHECK(encode(snapshot, bytes) == 0);

    snapshot = capture(Simulation(EdgeRule::WALLS, 9));
    snapshot.normal_since_potato = 1;
    CHECK(encode(snapshot, bytes) == 0);
}

TEST_CASE("Bytes of version 2 with something wrong in the variant part are refused", "[snapshot][format][variant]")
{
    const std::vector<std::uint8_t> good = encoded(capture(game_with_lump()));
    REQUIRE(decode(good).has_value());
    // The variant byte is 13 bytes from the end: the count, the flags and the bomb's two cells follow it.
    const std::size_t variant_at = good.size() - 5;
    REQUIRE(good[variant_at] == static_cast<std::uint8_t>(Variant::HOT_POTATO));

    const auto refused_with = [&](std::size_t position, std::uint8_t value)
    {
        std::vector<std::uint8_t> bytes = good;
        bytes[position] = value;
        return !decode(bytes).has_value();
    };
    CHECK(refused_with(variant_at, 0));     // the plain variant is written as version 1
    CHECK(refused_with(variant_at, 3));     // a variant that does not exist
    CHECK(refused_with(variant_at + 2, 4)); // a flag that does not exist
    CHECK(refused_with(0, 3));              // a version that does not exist

    for (std::size_t length = 0; length < good.size(); ++length)
    {
        CHECK_FALSE(decode(std::span<const std::uint8_t>(good.data(), length)).has_value());
    }
}

TEST_CASE("A game with portals is the same snapshot after it is saved and loaded", "[snapshot][variant]")
{
    Simulation game(EdgeRule::WALLS, 5, Variant::PORTALS);
    for (int tick = 0; tick < 12; ++tick)
    {
        static_cast<void>(game.tick());
    }
    const std::optional<Snapshot> back = decode(encoded(capture(game)));
    REQUIRE(back.has_value());
    const std::optional<Simulation> restored = restore(*back);
    REQUIRE(restored.has_value());
    CHECK(state_hash(*restored) == state_hash(game));
}
