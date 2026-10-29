// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/sim/snapshot.hpp"

#include "snake/sim/byte_io.hpp"
#include "snake/sim/fnv1a.hpp"

namespace snake::sim
{

namespace
{

// Version 1 is a game of the plain variant, version 2 any other (see encode() in the header).
constexpr std::uint8_t FORMAT_VERSION_PLAIN = 1;
constexpr std::uint8_t FORMAT_VERSION_VARIANT = 2;

constexpr std::uint8_t FLAG_RUNNING = 1U << 0U;
constexpr std::uint8_t FLAG_WON = 1U << 1U;
constexpr std::uint8_t FLAG_BONUS = 1U << 2U;
constexpr std::uint8_t KNOWN_FLAGS = FLAG_RUNNING | FLAG_WON | FLAG_BONUS;

// The flags of the variant part of version 2, for HOT_POTATO.
constexpr std::uint8_t POTATO_ON_BOARD = 1U << 0U;
constexpr std::uint8_t LUMP_IN_SNAKE = 1U << 1U;
constexpr std::uint8_t POTATO_KNOWN_FLAGS = POTATO_ON_BOARD | LUMP_IN_SNAKE;

void put_cell(ByteWriter& writer, Cell value) noexcept
{
    writer.u8(value.x);
    writer.u8(value.y);
}

Cell get_cell(ByteReader& reader) noexcept
{
    const std::uint8_t x = reader.u8();
    const std::uint8_t y = reader.u8();
    return Cell{x, y};
}

/// Reads what version 2 adds after the fields of version 1. False for the plain variant, which is never written as
/// version 2, for a variant that does not exist and for flags that do not exist.
bool get_variant_part(ByteReader& reader, Snapshot& snapshot) noexcept
{
    const std::uint8_t variant = reader.u8();
    if (variant != static_cast<std::uint8_t>(Variant::PORTALS) &&
        variant != static_cast<std::uint8_t>(Variant::HOT_POTATO))
    {
        return false;
    }
    snapshot.variant = static_cast<Variant>(variant);
    if (snapshot.variant == Variant::PORTALS)
    {
        Simulation::PortalCells portals;
        for (Cell& cell : portals.cells)
        {
            cell = get_cell(reader);
        }
        snapshot.portals = portals;
        return true;
    }
    snapshot.normal_since_potato = reader.u8();
    const std::uint8_t flags = reader.u8();
    if ((flags & ~POTATO_KNOWN_FLAGS) != 0)
    {
        return false;
    }
    if ((flags & POTATO_ON_BOARD) != 0)
    {
        const Cell cell = get_cell(reader);
        snapshot.potato = Simulation::Potato{.cell = cell, .ticks_left = reader.u8()};
    }
    if ((flags & LUMP_IN_SNAKE) != 0)
    {
        snapshot.lump = get_cell(reader);
    }
    return true;
}

} // namespace

Snapshot capture(const Simulation& game) noexcept
{
    Snapshot snapshot;
    snapshot.edges = game.edge_rule();
    snapshot.heading = game.heading();
    snapshot.running = game.running();
    snapshot.won = game.won();
    snapshot.ticks = game.ticks();
    snapshot.length = static_cast<std::uint16_t>(game.length());
    for (std::size_t part = 0; part < game.length(); ++part)
    {
        snapshot.snake[part] = game.snake()[part];
    }
    snapshot.food = game.food();
    snapshot.bonus = game.bonus();
    snapshot.scoring = game.scoring().state();
    snapshot.random = game.random_state();
    snapshot.pending_count = static_cast<std::uint8_t>(game.pending_turns().size());
    for (std::size_t place = 0; place < game.pending_turns().size(); ++place)
    {
        snapshot.pending[place] = game.pending_turns().at(place);
    }
    snapshot.variant = game.variant();
    snapshot.portals = game.portals();
    snapshot.potato = game.potato();
    snapshot.lump = game.lump();
    snapshot.normal_since_potato = game.normal_since_potato();
    return snapshot;
}

std::optional<Simulation> restore(const Snapshot& snapshot) noexcept
{
    if (snapshot.length > snapshot.snake.size() || snapshot.pending_count > snapshot.pending.size())
    {
        return std::nullopt;
    }
    return Simulation::from_setup(
        {.edges = snapshot.edges,
         .heading = snapshot.heading,
         .snake = std::span<const Cell>(snapshot.snake.data(), snapshot.length),
         .food = snapshot.food,
         .seed = 0,
         .scoring = snapshot.scoring,
         .bonus = snapshot.bonus,
         .ticks = snapshot.ticks,
         .random = snapshot.random,
         .pending = std::span<const Direction>(snapshot.pending.data(), snapshot.pending_count),
         .running = snapshot.running,
         .won = snapshot.won,
         .variant = snapshot.variant,
         .portals = snapshot.portals,
         .potato = snapshot.potato,
         .lump = snapshot.lump,
         .normal_since_potato = snapshot.normal_since_potato});
}

std::size_t encode(const Snapshot& snapshot, std::span<std::uint8_t> out) noexcept
{
    if (snapshot.length > snapshot.snake.size() || snapshot.pending_count > snapshot.pending.size())
    {
        return 0;
    }
    // The pieces of a variant are written for that variant only, so that a snapshot with pieces that do not belong to
    // its variant is not written (it could not be restored either).
    const bool plain = snapshot.variant == Variant::PLAIN;
    if ((snapshot.variant == Variant::PORTALS) != snapshot.portals.has_value() ||
        (snapshot.variant != Variant::HOT_POTATO &&
         (snapshot.potato || snapshot.lump || snapshot.normal_since_potato != 0)))
    {
        return 0;
    }
    ByteWriter writer(out);
    writer.u8(plain ? FORMAT_VERSION_PLAIN : FORMAT_VERSION_VARIANT);
    writer.u8(static_cast<std::uint8_t>(snapshot.edges));
    writer.u8(static_cast<std::uint8_t>(snapshot.heading));
    writer.u8(static_cast<std::uint8_t>((snapshot.running ? FLAG_RUNNING : 0U) | (snapshot.won ? FLAG_WON : 0U) |
                                        (snapshot.bonus ? FLAG_BONUS : 0U)));
    writer.u32(snapshot.ticks);
    writer.u16(snapshot.length);
    for (std::size_t part = 0; part < snapshot.length; ++part)
    {
        put_cell(writer, snapshot.snake[part]);
    }
    put_cell(writer, snapshot.food);
    if (snapshot.bonus)
    {
        put_cell(writer, snapshot.bonus->cell);
        writer.u8(snapshot.bonus->ticks_left);
    }
    writer.u32(snapshot.scoring.score);
    writer.u16(snapshot.scoring.foods_eaten);
    writer.u8(snapshot.scoring.normal_since_bonus);
    writer.u8(snapshot.scoring.combo);
    writer.u8(snapshot.scoring.ticks_since_food);
    writer.u64(snapshot.random.state);
    writer.u64(snapshot.random.increment);
    writer.u8(snapshot.pending_count);
    for (std::size_t place = 0; place < snapshot.pending_count; ++place)
    {
        writer.u8(static_cast<std::uint8_t>(snapshot.pending[place]));
    }
    if (!plain)
    {
        writer.u8(static_cast<std::uint8_t>(snapshot.variant));
        if (snapshot.portals)
        {
            for (const Cell cell : snapshot.portals->cells)
            {
                put_cell(writer, cell);
            }
        }
        else
        {
            writer.u8(snapshot.normal_since_potato);
            writer.u8(static_cast<std::uint8_t>((snapshot.potato ? POTATO_ON_BOARD : 0U) |
                                                (snapshot.lump ? LUMP_IN_SNAKE : 0U)));
            if (snapshot.potato)
            {
                put_cell(writer, snapshot.potato->cell);
                writer.u8(snapshot.potato->ticks_left);
            }
            if (snapshot.lump)
            {
                put_cell(writer, *snapshot.lump);
            }
        }
    }
    return writer.size();
}

std::optional<Snapshot> decode(std::span<const std::uint8_t> bytes) noexcept
{
    ByteReader reader(bytes);
    const std::uint8_t version = reader.u8();
    if (version != FORMAT_VERSION_PLAIN && version != FORMAT_VERSION_VARIANT)
    {
        return std::nullopt;
    }
    Snapshot snapshot;
    const std::uint8_t edges = reader.u8();
    const std::uint8_t heading = reader.u8();
    const std::uint8_t flags = reader.u8();
    if (edges > static_cast<std::uint8_t>(EdgeRule::WRAP) || heading > static_cast<std::uint8_t>(Direction::RIGHT) ||
        (flags & ~KNOWN_FLAGS) != 0)
    {
        return std::nullopt;
    }
    snapshot.edges = static_cast<EdgeRule>(edges);
    snapshot.heading = static_cast<Direction>(heading);
    snapshot.running = (flags & FLAG_RUNNING) != 0;
    snapshot.won = (flags & FLAG_WON) != 0;
    snapshot.ticks = reader.u32();
    snapshot.length = reader.u16();
    if (snapshot.length > snapshot.snake.size())
    {
        return std::nullopt;
    }
    for (std::size_t part = 0; part < snapshot.length; ++part)
    {
        snapshot.snake[part] = get_cell(reader);
    }
    snapshot.food = get_cell(reader);
    if ((flags & FLAG_BONUS) != 0)
    {
        const Cell cell = get_cell(reader);
        snapshot.bonus = Simulation::Bonus{.cell = cell, .ticks_left = reader.u8()};
    }
    snapshot.scoring.score = reader.u32();
    snapshot.scoring.foods_eaten = reader.u16();
    snapshot.scoring.normal_since_bonus = reader.u8();
    snapshot.scoring.combo = reader.u8();
    snapshot.scoring.ticks_since_food = reader.u8();
    snapshot.random.state = reader.u64();
    snapshot.random.increment = reader.u64();
    snapshot.pending_count = reader.u8();
    if (snapshot.pending_count > snapshot.pending.size())
    {
        return std::nullopt;
    }
    for (std::size_t place = 0; place < snapshot.pending_count; ++place)
    {
        const std::uint8_t turn = reader.u8();
        if (turn > static_cast<std::uint8_t>(Direction::RIGHT))
        {
            return std::nullopt;
        }
        snapshot.pending[place] = static_cast<Direction>(turn);
    }
    if (version == FORMAT_VERSION_VARIANT && !get_variant_part(reader, snapshot))
    {
        return std::nullopt;
    }
    if (!reader.complete())
    {
        return std::nullopt;
    }
    return snapshot;
}

std::uint64_t state_hash(const Snapshot& snapshot) noexcept
{
    std::array<std::uint8_t, MAX_ENCODED_SNAPSHOT_SIZE> bytes{};
    const std::size_t size = encode(snapshot, bytes);
    return fnv1a64(std::span<const std::uint8_t>(bytes.data(), size));
}

std::uint64_t state_hash(const Simulation& game) noexcept
{
    return state_hash(capture(game));
}

} // namespace snake::sim
