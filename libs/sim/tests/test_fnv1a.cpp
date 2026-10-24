// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/sim/fnv1a.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>

using snake::sim::fnv1a64;

namespace
{

template <std::size_t Size>
std::uint64_t hash_of(const char (&text)[Size])
{
    std::array<std::uint8_t, Size - 1> bytes{};
    for (std::size_t index = 0; index + 1 < Size; ++index)
    {
        bytes[index] = static_cast<std::uint8_t>(text[index]);
    }
    return fnv1a64(bytes);
}

} // namespace

TEST_CASE("FNV-1a gives the published 64-bit values", "[fnv1a]")
{
    CHECK(hash_of("") == 0xcbf29ce484222325ULL);
    CHECK(hash_of("a") == 0xaf63dc4c8601ec8cULL);
    CHECK(hash_of("foobar") == 0x85944171f73967e8ULL);
}

TEST_CASE("FNV-1a can be worked out at compile time", "[fnv1a]")
{
    constexpr std::array<std::uint8_t, 1> BYTES{'a'};
    static_assert(fnv1a64(BYTES) == 0xaf63dc4c8601ec8cULL);
    CHECK(fnv1a64(BYTES) == 0xaf63dc4c8601ec8cULL);
}

TEST_CASE("A different byte or a different order gives a different hash", "[fnv1a]")
{
    CHECK(hash_of("ab") != hash_of("ba"));
    CHECK(hash_of("ab") != hash_of("ac"));
}
