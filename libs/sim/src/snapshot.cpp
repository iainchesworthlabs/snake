// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/sim/snapshot.hpp"

#include "snake/sim/fnv1a.hpp"

namespace snake::sim
{

namespace
{

constexpr std::uint8_t FORMAT_VERSION = 1;

constexpr std::uint8_t FLAG_RUNNING = 1U << 0U;
constexpr std::uint8_t FLAG_WON = 1U << 1U;
constexpr std::uint8_t FLAG_BONUS = 1U << 2U;
constexpr std::uint8_t KNOWN_FLAGS = FLAG_RUNNING | FLAG_WON | FLAG_BONUS;

/// Appends bytes to a buffer and remembers when it ran out of room.
class Writer
{
public:
    explicit Writer(std::span<std::uint8_t> out) noexcept : m_Out(out) {}

    void u8(std::uint8_t value) noexcept
    {
        if (m_Size < m_Out.size())
        {
            m_Out[m_Size] = value;
        }
        ++m_Size;
    }

    void u16(std::uint16_t value) noexcept
    {
        u8(static_cast<std::uint8_t>(value));
        u8(static_cast<std::uint8_t>(value >> 8U));
    }

    void u32(std::uint32_t value) noexcept
    {
        u16(static_cast<std::uint16_t>(value));
        u16(static_cast<std::uint16_t>(value >> 16U));
    }

    void u64(std::uint64_t value) noexcept
    {
        u32(static_cast<std::uint32_t>(value));
        u32(static_cast<std::uint32_t>(value >> 32U));
    }

    void cell(Cell value) noexcept
    {
        u8(value.x);
        u8(value.y);
    }

    /// The bytes written, or 0 when they did not fit.
    [[nodiscard]] std::size_t size() const noexcept
    {
        return m_Size <= m_Out.size() ? m_Size : 0;
    }

private:
    std::span<std::uint8_t> m_Out;
    std::size_t m_Size = 0;
};

/// Reads bytes from a buffer and remembers when it ran past the end.
class Reader
{
public:
    explicit Reader(std::span<const std::uint8_t> bytes) noexcept : m_Bytes(bytes) {}

    std::uint8_t u8() noexcept
    {
        if (m_Position >= m_Bytes.size())
        {
            m_Short = true;
            return 0;
        }
        return m_Bytes[m_Position++];
    }

    std::uint16_t u16() noexcept
    {
        const std::uint16_t low = u8();
        const std::uint16_t high = u8();
        return static_cast<std::uint16_t>(low | (high << 8U));
    }

    std::uint32_t u32() noexcept
    {
        const std::uint32_t low = u16();
        const std::uint32_t high = u16();
        return low | (high << 16U);
    }

    std::uint64_t u64() noexcept
    {
        const std::uint64_t low = u32();
        const std::uint64_t high = u32();
        return low | (high << 32U);
    }

    Cell cell() noexcept
    {
        const std::uint8_t x = u8();
        const std::uint8_t y = u8();
        return Cell{x, y};
    }

    /// True when every byte read so far existed and none is left over.
    [[nodiscard]] bool complete() const noexcept
    {
        return !m_Short && m_Position == m_Bytes.size();
    }

private:
    std::span<const std::uint8_t> m_Bytes;
    std::size_t m_Position = 0;
    bool m_Short = false;
};

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
         .won = snapshot.won});
}

std::size_t encode(const Snapshot& snapshot, std::span<std::uint8_t> out) noexcept
{
    if (snapshot.length > snapshot.snake.size() || snapshot.pending_count > snapshot.pending.size())
    {
        return 0;
    }
    Writer writer(out);
    writer.u8(FORMAT_VERSION);
    writer.u8(static_cast<std::uint8_t>(snapshot.edges));
    writer.u8(static_cast<std::uint8_t>(snapshot.heading));
    writer.u8(static_cast<std::uint8_t>((snapshot.running ? FLAG_RUNNING : 0U) | (snapshot.won ? FLAG_WON : 0U) |
                                        (snapshot.bonus ? FLAG_BONUS : 0U)));
    writer.u32(snapshot.ticks);
    writer.u16(snapshot.length);
    for (std::size_t part = 0; part < snapshot.length; ++part)
    {
        writer.cell(snapshot.snake[part]);
    }
    writer.cell(snapshot.food);
    if (snapshot.bonus)
    {
        writer.cell(snapshot.bonus->cell);
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
    return writer.size();
}

std::optional<Snapshot> decode(std::span<const std::uint8_t> bytes) noexcept
{
    Reader reader(bytes);
    if (reader.u8() != FORMAT_VERSION)
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
        snapshot.snake[part] = reader.cell();
    }
    snapshot.food = reader.cell();
    if ((flags & FLAG_BONUS) != 0)
    {
        const Cell cell = reader.cell();
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
