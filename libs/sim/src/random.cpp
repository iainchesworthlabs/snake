// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/sim/random.hpp"

#include <bit>

namespace snake::sim
{

namespace
{
constexpr std::uint64_t MULTIPLIER = 6364136223846793005ULL;
} // namespace

Random::Random(std::uint64_t seed, std::uint64_t stream) noexcept : m_Increment((stream << 1) | 1)
{
    // The seeding steps of the reference implementation.
    static_cast<void>(next());
    m_State += seed;
    static_cast<void>(next());
}

std::uint32_t Random::next() noexcept
{
    const std::uint64_t old_state = m_State;
    m_State = old_state * MULTIPLIER + m_Increment;
    const auto xor_shifted = static_cast<std::uint32_t>(((old_state >> 18U) ^ old_state) >> 27U);
    const auto rotation = static_cast<int>(old_state >> 59U);
    return std::rotr(xor_shifted, rotation);
}

std::uint32_t Random::below(std::uint32_t bound) noexcept
{
    if (bound == 0)
    {
        return 0;
    }
    // Numbers below this threshold would make the low results more likely, so they are skipped.
    const std::uint32_t threshold = (0U - bound) % bound;
    for (;;)
    {
        const std::uint32_t value = next();
        if (value >= threshold)
        {
            return value % bound;
        }
    }
}

} // namespace snake::sim
