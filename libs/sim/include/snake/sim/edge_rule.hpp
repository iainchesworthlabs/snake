// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>

namespace snake::sim
{

/// What happens at the edge of the board. Classic ends the game on a wall. Easy wraps to the opposite edge.
enum class EdgeRule : std::uint8_t
{
    WALLS,
    WRAP,
};

} // namespace snake::sim
