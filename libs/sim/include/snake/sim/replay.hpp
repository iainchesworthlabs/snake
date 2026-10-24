// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/sim/input_log.hpp"
#include "snake/sim/mode.hpp"
#include "snake/sim/session.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace snake::sim
{

/// A recorded game: the mode, the seed and every turn, with the tick count, score and state hash the game ended on.
/// The same mode, seed and turns give the same game on every platform, so playing a replay and comparing the
/// results with these shows that the rules still behave as they did when it was recorded. That is what the golden
/// replay tests do.
///
/// A Replay holds a whole input log (about 64 KB), so on a small stack keep one in static storage.
struct Replay
{
    Mode mode = Mode::CLASSIC;
    std::uint64_t seed = 0;
    std::uint32_t ticks = 0;      // ticks played
    std::uint32_t score = 0;      // the score at the end
    std::uint64_t final_hash = 0; // state_hash() of the game at the end
    InputLog inputs;

    friend bool operator==(const Replay& lhs, const Replay& rhs) noexcept = default;
};

/// What playing a replay gave.
struct ReplayOutcome
{
    std::uint32_t ticks = 0;
    std::uint32_t score = 0;
    std::uint64_t hash = 0;
    GameState state = GameState::NOT_STARTED;
};

/// Plays `replay` from the start: starts a session in its mode with its seed, gives each turn to the game just before
/// the tick it belongs to, and stops after `replay.ticks` ticks or when the game ends. The ticks, score and hash it
/// reports are those of the game it played, not those stored in the replay.
[[nodiscard]] ReplayOutcome play(const Replay& replay) noexcept;

/// True when playing `replay` ends with the ticks, score and hash it carries.
[[nodiscard]] bool verify(const Replay& replay) noexcept;

/// A game as it is played, with its turns written down. It has the same moves as Session, and a turn that the game
/// accepts goes into the log. The player's own pauses are not recorded because a replay has no clock.
class Recording
{
public:
    Recording(Mode mode, std::uint64_t seed) noexcept;

    bool start() noexcept
    {
        return m_Session.start();
    }

    bool pause() noexcept
    {
        return m_Session.pause();
    }

    bool resume() noexcept
    {
        return m_Session.resume();
    }

    /// Like Session::turn. An accepted turn is written down when the log has room. A turn that does not fit still
    /// steers the game, and the recording can no longer be turned into a replay.
    bool turn(Direction input) noexcept;

    [[nodiscard]] TickOutcome tick() noexcept
    {
        return m_Session.tick();
    }

    [[nodiscard]] const Session& session() const noexcept
    {
        return m_Session;
    }

    /// False once a turn did not fit in the log.
    [[nodiscard]] bool complete() const noexcept
    {
        return !m_Overflowed;
    }

    /// The replay of the game as it is now, or nothing when a turn did not fit in the log.
    [[nodiscard]] std::optional<Replay> finish() const noexcept;

private:
    Session m_Session;
    std::uint64_t m_Seed;
    InputLog m_Log;
    bool m_Overflowed = false;
};

/// The most bytes encode_replay() writes.
inline constexpr std::size_t MAX_ENCODED_REPLAY_SIZE = 64 + (InputLog::CAPACITY * 6);

/// Writes `replay` as bytes, the same on every platform. The file is: the letters SNKR, a version byte, the mode,
/// the seed (8 bytes), the tick count, the score, the final hash (8 bytes), the number of turns, then each turn as
/// the ticks since the turn before it (in 7-bit groups, low group first, the top bit of a byte set when another
/// follows) and a direction byte. Integers are little-endian. Returns the number of bytes written, or 0 when `out`
/// is too small (MAX_ENCODED_REPLAY_SIZE is always enough).
[[nodiscard]] std::size_t encode_replay(const Replay& replay, std::span<std::uint8_t> out) noexcept;

/// Reads bytes written by encode_replay(). Returns nothing for bytes that are cut short or have bytes left over, a
/// wrong marker or version, an unknown mode or direction, more turns than a log holds, a turn number that does not
/// fit in 32 bits or lies after the last tick, and a number written in more than 5 groups.
[[nodiscard]] std::optional<Replay> decode_replay(std::span<const std::uint8_t> bytes) noexcept;

} // namespace snake::sim
