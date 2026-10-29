// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/sim/replay.hpp"

#include "snake/sim/byte_io.hpp"
#include "snake/sim/snapshot.hpp"

#include <array>

namespace snake::sim
{

namespace
{

constexpr std::array<std::uint8_t, 4> MARKER{'S', 'N', 'K', 'R'};
constexpr std::uint8_t FORMAT_VERSION = 1;
// The byte of an input that is a shed. The directions are 0 to 3.
constexpr std::uint8_t SHED_BYTE = 4;

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
            static_cast<void>(events[next].shed ? session.shed() : session.turn(events[next].direction));
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

bool Recording::shed() noexcept
{
    if (!m_Session.shed())
    {
        return false;
    }
    if (!m_Log.record_shed(m_Session.simulation().ticks()))
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
    ByteWriter writer(out);
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
        writer.u8(event.shed ? SHED_BYTE : static_cast<std::uint8_t>(event.direction));
        previous = event.tick;
    }
    return writer.size();
}

std::optional<Replay> decode_replay(std::span<const std::uint8_t> bytes) noexcept
{
    ByteReader reader(bytes);
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
    if (mode >= MODE_COUNT)
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
        if (reader.bad() || tick > replay.ticks || direction > SHED_BYTE)
        {
            return std::nullopt;
        }
        static_cast<void>(direction == SHED_BYTE ? replay.inputs.record_shed(static_cast<std::uint32_t>(tick))
                                                 : replay.inputs.record(static_cast<std::uint32_t>(tick),
                                                                        static_cast<Direction>(direction)));
    }
    if (!reader.complete())
    {
        return std::nullopt;
    }
    return replay;
}

} // namespace snake::sim
