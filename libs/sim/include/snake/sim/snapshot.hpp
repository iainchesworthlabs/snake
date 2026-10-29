// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/sim/cell.hpp"
#include "snake/sim/direction.hpp"
#include "snake/sim/edge_rule.hpp"
#include "snake/sim/random.hpp"
#include "snake/sim/scoring.hpp"
#include "snake/sim/simulation.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace snake::sim
{

/// All of the state of a game at one moment: enough to carry on from there, to save a game, and to compare two games.
/// Two games with equal snapshots play on identically.
struct Snapshot
{
    EdgeRule edges = EdgeRule::WALLS;
    Direction heading = Direction::RIGHT;
    bool running = true;
    bool won = false;
    std::uint32_t ticks = 0;
    std::uint16_t length = 0;              // how many entries of `snake` are in use
    std::array<Cell, BOARD_CELLS> snake{}; // head first
    Cell food;
    std::optional<Simulation::Bonus> bonus;
    Scoring::State scoring;
    Random::State random;
    std::uint8_t pending_count = 0;                         // turns waiting in the input buffer
    std::array<Direction, InputBuffer::CAPACITY> pending{}; // next first
    Variant variant = Variant::PLAIN;
    std::optional<Simulation::PortalCells> portals; // exactly when the variant is PORTALS
    std::optional<Simulation::Potato> potato;       // HOT_POTATO: the bomb food on the board
    std::optional<Cell> lump;                       // HOT_POTATO: the bomb in the snake
    std::uint8_t normal_since_potato = 0;           // HOT_POTATO: normal foods eaten towards the next bomb food

    friend bool operator==(const Snapshot& lhs, const Snapshot& rhs) noexcept = default;
};

/// The snapshot of `game` as it is now.
[[nodiscard]] Snapshot capture(const Simulation& game) noexcept;

/// The game that `snapshot` describes, or nothing when it is impossible (see Simulation::from_setup).
[[nodiscard]] std::optional<Simulation> restore(const Snapshot& snapshot) noexcept;

/// The most bytes encode() writes: the snake at full length and everything else.
inline constexpr std::size_t MAX_ENCODED_SNAPSHOT_SIZE = 1024;

/// Writes the snapshot as bytes, the same on every platform: a version byte, the fields in order, integers in
/// little-endian order, the snake as a length and that many cells. A game of the plain variant is written as version 1
/// always, as it was before there were variants, so that the hash of such a game never changes. A game of another
/// variant is written as version 2: the fields of version 1, then the variant and what that variant adds (the four
/// portals; or the count of foods towards the next bomb food, then which of the bomb food and the bomb in the snake
/// there are, and where). Returns the number of bytes written, or 0 when `out` is too small (MAX_ENCODED_SNAPSHOT_SIZE
/// always is enough) or the snapshot's length or pending count is above what it can hold.
[[nodiscard]] std::size_t encode(const Snapshot& snapshot, std::span<std::uint8_t> out) noexcept;

/// Reads bytes written by encode(), of version 1 or 2. Returns nothing for bytes that are cut short, have bytes left
/// over, have a version this code does not know, or hold a value that no snapshot has (a direction above 3, a snake
/// longer than the board). Whether the game is possible is for restore() to say.
[[nodiscard]] std::optional<Snapshot> decode(std::span<const std::uint8_t> bytes) noexcept;

/// The hash of a game's state: FNV-1a over the encoded snapshot. Equal states give equal hashes on every platform,
/// which makes it the value a golden replay is checked against.
[[nodiscard]] std::uint64_t state_hash(const Snapshot& snapshot) noexcept;
[[nodiscard]] std::uint64_t state_hash(const Simulation& game) noexcept;

} // namespace snake::sim
