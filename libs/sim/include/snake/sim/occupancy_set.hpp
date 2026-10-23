// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

namespace snake::sim
{

/// A set of the integers 0 to Bits - 1, one bit each, with the storage inside the object. The board uses one to
/// record which cells the snake covers. Indices at or above Bits are never members: set() and reset() ignore
/// them and test() returns false.
template <std::size_t Bits>
class OccupancySet
{
public:
    constexpr void set(std::size_t index) noexcept
    {
        if (index < Bits)
        {
            m_Words[index / WORD_BITS] |= bit(index);
        }
    }

    constexpr void reset(std::size_t index) noexcept
    {
        if (index < Bits)
        {
            m_Words[index / WORD_BITS] &= ~bit(index);
        }
    }

    [[nodiscard]] constexpr bool test(std::size_t index) const noexcept
    {
        return index < Bits && (m_Words[index / WORD_BITS] & bit(index)) != 0;
    }

    constexpr void clear() noexcept
    {
        m_Words.fill(0);
    }

    /// The number of members.
    [[nodiscard]] constexpr std::size_t count() const noexcept
    {
        std::size_t total = 0;
        for (const std::uint64_t word : m_Words)
        {
            total += static_cast<std::size_t>(std::popcount(word));
        }
        return total;
    }

    /// The `rank`-th index (counting from 0) that is not a member, or Bits when there are fewer than rank + 1.
    /// This is how a uniform random empty cell is chosen without building a list of them.
    [[nodiscard]] constexpr std::size_t nth_clear(std::size_t rank) const noexcept
    {
        for (std::size_t word_index = 0; word_index < m_Words.size(); ++word_index)
        {
            const std::size_t first = word_index * WORD_BITS;
            const std::size_t in_word = first + WORD_BITS <= Bits ? WORD_BITS : Bits - first;
            std::uint64_t clear_bits = ~m_Words[word_index];
            if (in_word < WORD_BITS)
            {
                clear_bits &= (std::uint64_t{1} << in_word) - 1;
            }
            const auto available = static_cast<std::size_t>(std::popcount(clear_bits));
            if (rank >= available)
            {
                rank -= available;
                continue;
            }
            for (;; --rank)
            {
                const auto position = static_cast<std::size_t>(std::countr_zero(clear_bits));
                if (rank == 0)
                {
                    return first + position;
                }
                clear_bits &= clear_bits - 1;
            }
        }
        return Bits;
    }

private:
    static constexpr std::size_t WORD_BITS = 64;

    [[nodiscard]] static constexpr std::uint64_t bit(std::size_t index) noexcept
    {
        return std::uint64_t{1} << (index % WORD_BITS);
    }

    std::array<std::uint64_t, (Bits + WORD_BITS - 1) / WORD_BITS> m_Words{};
};

} // namespace snake::sim
