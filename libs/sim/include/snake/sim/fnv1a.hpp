// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <span>

namespace snake::sim
{

/// The 64-bit FNV-1a hash of `bytes`. It is a checksum for comparing game states and replays, not a defence
/// against tampering. The result is the same on every platform.
[[nodiscard]] constexpr std::uint64_t fnv1a64(std::span<const std::uint8_t> bytes) noexcept
{
    constexpr std::uint64_t OFFSET_BASIS = 14695981039346656037ULL;
    constexpr std::uint64_t PRIME = 1099511628211ULL;
    std::uint64_t hash = OFFSET_BASIS;
    for (const std::uint8_t byte : bytes)
    {
        hash ^= byte;
        hash *= PRIME;
    }
    return hash;
}

} // namespace snake::sim
