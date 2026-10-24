// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>

namespace snake::sim
{

/// A seeded random number generator that gives the same numbers on every platform: PCG32 (XSH RR), by Melissa
/// O'Neill. The standard library's generators and distributions are not used because their output differs between
/// implementations, and a replay must give the same game everywhere.
class Random
{
public:
    /// The seed chooses where in the sequence to start. `stream` chooses one of 2^63 independent sequences.
    explicit Random(std::uint64_t seed, std::uint64_t stream = DEFAULT_STREAM) noexcept;

    /// Everything the generator remembers, to save a game and restore it. The increment is always odd.
    struct State
    {
        std::uint64_t state = 0;
        std::uint64_t increment = 1;

        friend constexpr bool operator==(const State& lhs, const State& rhs) noexcept = default;
    };

    [[nodiscard]] State state() const noexcept
    {
        return {.state = m_State, .increment = m_Increment};
    }

    /// A generator that carries on from `state`, whose increment must be odd.
    explicit Random(const State& state) noexcept : m_State(state.state), m_Increment(state.increment) {}

    /// The next 32 random bits.
    [[nodiscard]] std::uint32_t next() noexcept;

    /// A number from 0 to bound - 1, each equally likely (no modulo bias). Returns 0 when bound is 0.
    [[nodiscard]] std::uint32_t below(std::uint32_t bound) noexcept;

private:
    static constexpr std::uint64_t DEFAULT_STREAM = 54;

    std::uint64_t m_State = 0;
    std::uint64_t m_Increment = 0;
};

} // namespace snake::sim
