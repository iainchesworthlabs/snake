// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/sim/replay.hpp"

#include "snake/sim/snapshot.hpp"

#include <array>

namespace snake::sim
{

namespace
{

constexpr std::array<std::uint8_t, 4> MARKER{'S', 'N', 'K', 'R'};
constexpr std::uint8_t FORMAT_VERSION = 1;
constexpr std::size_t MAX_VARINT_BYTES = 5;

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

    void u32(std::uint32_t value) noexcept
    {
        for (unsigned shift = 0; shift < 32; shift += 8)
        {
            u8(static_cast<std::uint8_t>(value >> shift));
        }
    }

    void u64(std::uint64_t value) noexcept
    {
        u32(static_cast<std::uint32_t>(value));
        u32(static_cast<std::uint32_t>(value >> 32U));
    }

    /// A number in groups of 7 bits, low group first. A byte has its top bit set when another follows.
    void varint(std::uint32_t value) noexcept
    {
        while (value >= 0x80U)
        {
            u8(static_cast<std::uint8_t>((value & 0x7FU) | 0x80U));
            value >>= 7U;
        }
        u8(static_cast<std::uint8_t>(value));
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        return m_Size <= m_Out.size() ? m_Size : 0;
    }

private:
    std::span<std::uint8_t> m_Out;
    std::size_t m_Size = 0;
};

class Reader
{
public:
    explicit Reader(std::span<const std::uint8_t> bytes) noexcept : m_Bytes(bytes) {}

    std::uint8_t u8() noexcept
    {
        if (m_Position >= m_Bytes.size())
        {
            m_Bad = true;
            return 0;
        }
        return m_Bytes[m_Position++];
    }

    std::uint32_t u32() noexcept
    {
        std::uint32_t value = 0;
        for (unsigned shift = 0; shift < 32; shift += 8)
        {
            value |= static_cast<std::uint32_t>(u8()) << shift;
        }
        return value;
    }

    std::uint64_t u64() noexcept
    {
        const std::uint64_t low = u32();
        const std::uint64_t high = u32();
        return low | (high << 32U);
    }

    std::uint32_t varint() noexcept
    {
        std::uint64_t value = 0;
        for (std::size_t group = 0; group < MAX_VARINT_BYTES; ++group)
        {
            const std::uint8_t byte = u8();
            value |= static_cast<std::uint64_t>(byte & 0x7FU) << (7 * group);
            if ((byte & 0x80U) == 0)
            {
                if (value > 0xFFFFFFFFULL)
                {
                    m_Bad = true;
                }
                return static_cast<std::uint32_t>(value);
            }
        }
        // A sixth group is one too many for a 32-bit number.
        m_Bad = true;
        return 0;
    }

    /// True when every byte read so far existed and none is left over.
    [[nodiscard]] bool complete() const noexcept
    {
        return !m_Bad && m_Position == m_Bytes.size();
    }

    [[nodiscard]] bool bad() const noexcept
    {
        return m_Bad;
    }

private:
    std::span<const std::uint8_t> m_Bytes;
    std::size_t m_Position = 0;
    bool m_Bad = false;
};

} // namespace

ReplayOutcome play(const Replay& replay) noexcept
{
    Session session(replay.mode, replay.seed);
    static_cast<void>(session.start());
    const std::span<const InputEvent> events = replay.inputs.events();
    std::size_t next = 0;
    const auto give_turns_before = [&](std::uint32_t tick)
    {
        while (next < events.size() && events[next].tick == tick)
        {
            static_cast<void>(session.turn(events[next].direction));
            ++next;
        }
    };
    while (session.state() == GameState::RUNNING && session.simulation().ticks() < replay.ticks)
    {
        give_turns_before(session.simulation().ticks());
        static_cast<void>(session.tick());
    }
    // Turns made after the last tick are still waiting in the input buffer, and the hash includes them.
    give_turns_before(replay.ticks);
    return {.ticks = session.simulation().ticks(),
            .score = session.simulation().score(),
            .hash = state_hash(session.simulation()),
            .state = session.state()};
}

bool verify(const Replay& replay) noexcept
{
    const ReplayOutcome outcome = play(replay);
    return outcome.ticks == replay.ticks && outcome.score == replay.score && outcome.hash == replay.final_hash;
}

Recording::Recording(Mode mode, std::uint64_t seed) noexcept : m_Session(mode, seed), m_Seed(seed) {}

bool Recording::turn(Direction input) noexcept
{
    if (!m_Session.turn(input))
    {
        return false;
    }
    if (!m_Log.record(m_Session.simulation().ticks(), input))
    {
        m_Overflowed = true;
    }
    return true;
}

std::optional<Replay> Recording::finish() const noexcept
{
    if (m_Overflowed)
    {
        return std::nullopt;
    }
    Replay replay;
    replay.mode = m_Session.mode();
    replay.seed = m_Seed;
    replay.ticks = m_Session.simulation().ticks();
    replay.score = m_Session.simulation().score();
    replay.final_hash = state_hash(m_Session.simulation());
    replay.inputs = m_Log;
    return replay;
}

std::size_t encode_replay(const Replay& replay, std::span<std::uint8_t> out) noexcept
{
    Writer writer(out);
    for (const std::uint8_t letter : MARKER)
    {
        writer.u8(letter);
    }
    writer.u8(FORMAT_VERSION);
    writer.u8(static_cast<std::uint8_t>(replay.mode));
    writer.u64(replay.seed);
    writer.u32(replay.ticks);
    writer.u32(replay.score);
    writer.u64(replay.final_hash);
    writer.u32(static_cast<std::uint32_t>(replay.inputs.size()));
    std::uint32_t previous = 0;
    for (const InputEvent& event : replay.inputs.events())
    {
        writer.varint(event.tick - previous);
        writer.u8(static_cast<std::uint8_t>(event.direction));
        previous = event.tick;
    }
    return writer.size();
}

std::optional<Replay> decode_replay(std::span<const std::uint8_t> bytes) noexcept
{
    Reader reader(bytes);
    for (const std::uint8_t letter : MARKER)
    {
        if (reader.u8() != letter)
        {
            return std::nullopt;
        }
    }
    if (reader.u8() != FORMAT_VERSION)
    {
        return std::nullopt;
    }
    Replay replay;
    const std::uint8_t mode = reader.u8();
    if (mode > static_cast<std::uint8_t>(Mode::CLASSIC))
    {
        return std::nullopt;
    }
    replay.mode = static_cast<Mode>(mode);
    replay.seed = reader.u64();
    replay.ticks = reader.u32();
    replay.score = reader.u32();
    replay.final_hash = reader.u64();
    const std::uint32_t count = reader.u32();
    if (count > InputLog::CAPACITY)
    {
        return std::nullopt;
    }
    std::uint64_t tick = 0;
    for (std::uint32_t index = 0; index < count; ++index)
    {
        tick += reader.varint();
        const std::uint8_t direction = reader.u8();
        if (reader.bad() || tick > replay.ticks || direction > static_cast<std::uint8_t>(Direction::RIGHT))
        {
            return std::nullopt;
        }
        static_cast<void>(replay.inputs.record(static_cast<std::uint32_t>(tick), static_cast<Direction>(direction)));
    }
    if (!reader.complete())
    {
        return std::nullopt;
    }
    return replay;
}

} // namespace snake::sim
