// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>

namespace snake::sim
{

/// The extra rules a game plays by, besides the edge rule. A mode picks one (see variant_of() in mode.hpp). They are
/// the mutators of the requirements, and every rule they add is in Simulation.
enum class Variant : std::uint8_t
{
    PLAIN,      // food and a bonus item
    PORTALS,    // two pairs of portals on the board: a head that enters one comes out of the other of its pair
    HOT_POTATO, // bomb food that, once swallowed, rolls down the body and has to be shed before it goes off
};

} // namespace snake::sim
