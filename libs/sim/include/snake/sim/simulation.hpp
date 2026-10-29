// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/sim/cell.hpp"
#include "snake/sim/direction.hpp"
#include "snake/sim/edge_rule.hpp"
#include "snake/sim/input_buffer.hpp"
#include "snake/sim/occupancy_set.hpp"
#include "snake/sim/random.hpp"
#include "snake/sim/ring_buffer.hpp"
#include "snake/sim/scoring.hpp"
#include "snake/sim/variant.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace snake::sim
{

/// What one call to tick() did.
enum class TickOutcome : std::uint8_t
{
    MOVED,       // the snake moved one cell
    ATE,         // the snake moved onto the food and grew
    ATE_BONUS,   // the snake moved onto the bonus item and grew
    TELEPORTED,  // the head went into a portal and came out of the other one of its pair
    ATE_POTATO,  // the snake moved onto the bomb food and grew; the bomb is in its body now
    HIT_WALL,    // the next cell is off the board and the edge rule is WALLS: game over
    HIT_SELF,    // the next cell is part of the snake's body: game over
    EXPLODED,    // the bomb reached the tail before it was shed: game over
    WON,         // the snake ate and no empty cell is left for new food: the game ends as a perfect game
    NOT_RUNNING, // the game had already ended, nothing changed
};

/// The rules of one game of Snake on the 20 by 20 board. This is the whole game state and every rule; there is no
/// clock, no allocation and no platform code in it. The same seed and the same inputs on each tick give the same
/// game on every platform.
class Simulation
{
public:
    static constexpr std::size_t INITIAL_LENGTH = 3;

    /// How many ticks a bonus item stays on the board after it appears.
    static constexpr std::uint8_t BONUS_LIFETIME_TICKS = 40;

    /// The bonus item: extra food that appears after every fifth normal food and goes away on its own.
    struct Bonus
    {
        Cell cell;
        std::uint8_t ticks_left = BONUS_LIFETIME_TICKS; // 1 to BONUS_LIFETIME_TICKS; it is gone when this runs out

        friend constexpr bool operator==(const Bonus& lhs, const Bonus& rhs) noexcept = default;
    };

    /// How many ticks the bomb food stays on the board after it appears.
    static constexpr std::uint8_t POTATO_LIFETIME_TICKS = 50;

    /// A bomb food comes after every this many normal foods, once the snake is POTATO_MIN_LENGTH long and no bomb is on
    /// the board or in the snake.
    static constexpr std::uint8_t POTATO_EVERY = 3;
    static constexpr std::size_t POTATO_MIN_LENGTH = 6;

    /// How many portals a game with portals has: two pairs, the first two cells of PortalCells being one pair and the
    /// last two the other.
    static constexpr std::size_t PORTAL_COUNT = 4;

    /// The cells of the portals. A head that enters one comes out of the one it is paired with, and goes on from there
    /// in the direction it had.
    struct PortalCells
    {
        std::array<Cell, PORTAL_COUNT> cells{};

        /// The cell that the portal on `cell` leads to, or nothing when there is no portal on `cell`.
        [[nodiscard]] constexpr std::optional<Cell> exit_from(Cell cell) const noexcept
        {
            for (std::size_t index = 0; index < PORTAL_COUNT; ++index)
            {
                if (cells[index] == cell)
                {
                    return cells[index ^ 1U];
                }
            }
            return std::nullopt;
        }

        [[nodiscard]] constexpr bool contains(Cell cell) const noexcept
        {
            return exit_from(cell).has_value();
        }

        friend constexpr bool operator==(const PortalCells& lhs, const PortalCells& rhs) noexcept = default;
    };

    /// The bomb food on the board. It goes away on its own when its time runs out.
    struct Potato
    {
        Cell cell;
        std::uint8_t ticks_left = POTATO_LIFETIME_TICKS; // 1 to POTATO_LIFETIME_TICKS

        friend constexpr bool operator==(const Potato& lhs, const Potato& rhs) noexcept = default;
    };

    /// A new game: a snake of INITIAL_LENGTH in the middle of the board heading right, and the first food. With
    /// Variant::PORTALS the portals are placed first, with the game's generator.
    Simulation(EdgeRule edges, std::uint64_t seed, Variant variant = Variant::PLAIN) noexcept;

    /// A game in a given position, for tests and for restoring a saved game. The snake is listed head first.
    struct Setup
    {
        EdgeRule edges = EdgeRule::WALLS;
        Direction heading = Direction::RIGHT;
        std::span<const Cell> snake;
        Cell food;
        std::uint64_t seed = 0;
        Scoring::State scoring{};
        std::optional<Bonus> bonus{};
        std::uint32_t ticks = 0;               // ticks played so far
        std::optional<Random::State> random{}; // carries the generator on from here instead of seeding it
        std::span<const Direction> pending{};  // turns waiting in the input buffer, next first
        bool running = true;
        bool won = false;
        Variant variant = Variant::PLAIN;
        std::optional<PortalCells> portals{}; // exactly when the variant is PORTALS
        std::optional<Potato> potato{};       // bomb food on the board (HOT_POTATO only)
        std::optional<Cell> lump{};           // the bomb inside the snake, on a cell of its body (HOT_POTATO only)
        std::uint8_t normal_since_potato = 0; // 0 to POTATO_EVERY (HOT_POTATO only)
    };

    /// Builds the game in `setup`, or returns nothing when the position is impossible: a cell off the board, a cell
    /// used twice, cells that do not form a connected snake, a heading that points into the snake's neck, food (unless
    /// the game is won) or a bonus item on the snake or on each other, scoring that cannot happen, waiting turns the
    /// input buffer would not have taken, an even generator increment, or a win without a full board. Also a variant
    /// whose pieces are missing or that holds the pieces of another: portals that are not four different cells with
    /// no food, bonus item or bomb on them, a bomb in the snake and on the board at once, a bomb that is not on the
    /// body. In a snake with portals two cells next to each other in the list may be a portal apart instead of a step.
    [[nodiscard]] static std::optional<Simulation> from_setup(const Setup& setup) noexcept;

    /// Queues a direction for the next ticks. Returns false when the input is dropped (see InputBuffer).
    bool turn(Direction input) noexcept;

    /// Advances the game by one tick. The order is: start the combo clock; take the next queued direction; find the
    /// new head cell, with the edge rule; end the game on a wall; if that cell is a portal, the head lands on the cell
    /// of the other portal of its pair instead; end the game if the head lands on the body (the tail cell counts as
    /// free because it moves away, unless the snake is eating); end it if the bomb is on the tail cell and the tail is
    /// about to move away; add the head and, unless eating, remove the tail; score what was eaten; count down the bonus
    /// item and the bomb food, which an eating snake reaches first; after normal food, place new food, end the game as
    /// a win if there is no room for it, and add a bonus item or a bomb food when one is due.
    [[nodiscard]] TickOutcome tick() noexcept;

    /// Sheds the tail from the bomb on: the bomb and every part of the body behind it are dropped and their cells are
    /// free. It takes effect at once, between ticks. Returns false, changing nothing, when the game is not running,
    /// there is no bomb in the snake, or the bomb is still in the head (shedding there would drop the whole snake).
    bool shed() noexcept;

    /// How many ticks have been played. A tick that ends the game counts, and later calls do not.
    [[nodiscard]] std::uint32_t ticks() const noexcept
    {
        return m_Ticks;
    }

    /// The turns waiting for the next ticks.
    [[nodiscard]] const InputBuffer& pending_turns() const noexcept
    {
        return m_Input;
    }

    /// Where the food generator is, so that a saved game carries on with the same food.
    [[nodiscard]] Random::State random_state() const noexcept
    {
        return m_Random.state();
    }

    [[nodiscard]] bool running() const noexcept
    {
        return m_Running;
    }

    /// True when the game ended because the snake filled the board.
    [[nodiscard]] bool won() const noexcept
    {
        return m_Won;
    }

    [[nodiscard]] EdgeRule edge_rule() const noexcept
    {
        return m_Edges;
    }

    /// The direction the snake travels now. Queued turns are not included.
    [[nodiscard]] Direction heading() const noexcept
    {
        return m_Heading;
    }

    /// The snake, head first.
    [[nodiscard]] const RingBuffer<Cell, BOARD_CELLS>& snake() const noexcept
    {
        return m_Snake;
    }

    [[nodiscard]] Cell head() const noexcept
    {
        return m_Snake.front();
    }

    [[nodiscard]] std::size_t length() const noexcept
    {
        return m_Snake.size();
    }

    /// The food. Meaningless after a win, when there is no room for it.
    [[nodiscard]] Cell food() const noexcept
    {
        return m_Food;
    }

    /// The bonus item, or nothing when there is none on the board.
    [[nodiscard]] const std::optional<Bonus>& bonus() const noexcept
    {
        return m_Bonus;
    }

    [[nodiscard]] Variant variant() const noexcept
    {
        return m_Variant;
    }

    /// The portals, or nothing in a game without them.
    [[nodiscard]] const std::optional<PortalCells>& portals() const noexcept
    {
        return m_Portals;
    }

    /// The bomb food on the board, or nothing.
    [[nodiscard]] const std::optional<Potato>& potato() const noexcept
    {
        return m_Potato;
    }

    /// The cell of the body that the bomb is in, or nothing when none was swallowed. It does not move: the body moves
    /// past it, and the bomb is nearer the tail with every tick, and goes off when it is on the tail cell as the tail
    /// would move away.
    [[nodiscard]] const std::optional<Cell>& lump() const noexcept
    {
        return m_Lump;
    }

    /// How far the bomb is from the head, in parts of the body (0 is the head), or nothing when there is none.
    [[nodiscard]] std::optional<std::size_t> lump_index() const noexcept;

    /// How many normal foods have been eaten towards the next bomb food, 0 to POTATO_EVERY.
    [[nodiscard]] std::uint8_t normal_since_potato() const noexcept
    {
        return m_NormalSincePotato;
    }

    [[nodiscard]] const Scoring& scoring() const noexcept
    {
        return m_Scoring;
    }

    [[nodiscard]] std::uint32_t score() const noexcept
    {
        return m_Scoring.score();
    }

    /// The food multiplier now, 1 to 5.
    [[nodiscard]] std::uint8_t combo() const noexcept
    {
        return m_Scoring.combo();
    }

    /// Normal and bonus foods eaten. The snake is this much longer than INITIAL_LENGTH, and the tick gets shorter
    /// with it (see tick_interval_ms).
    [[nodiscard]] std::size_t foods_eaten() const noexcept
    {
        return m_Scoring.foods_eaten();
    }

private:
    Simulation() noexcept = default;

    /// Puts the food on an empty cell chosen uniformly with the game's generator, never on the bonus item, the bomb
    /// food or a portal. A bonus item on the last empty cell is removed and the food takes the cell. False when no cell
    /// is empty.
    [[nodiscard]] bool place_food() noexcept;

    /// Puts the bomb food on an empty cell other than the food's, the bonus item's and a portal, or does nothing when
    /// there is none.
    void place_potato() noexcept;

    /// Puts the four portals on cells that are empty, not in the row the snake starts in and not next to one another,
    /// with the game's generator.
    void place_portals() noexcept;

    /// The cells that nothing new may be placed on: the snake, the portals, the bomb food and the extra items that are
    /// named. The food is not in it.
    [[nodiscard]] OccupancySet<BOARD_CELLS> blocked_cells() const noexcept;

    /// Puts a bonus item on an empty cell other than the food's, or does nothing when fewer than two cells are
    /// empty. It replaces a bonus item that is still on the board.
    void place_bonus() noexcept;

    EdgeRule m_Edges = EdgeRule::WALLS;
    Variant m_Variant = Variant::PLAIN;
    std::optional<PortalCells> m_Portals;
    std::optional<Potato> m_Potato;
    std::optional<Cell> m_Lump;
    std::uint8_t m_NormalSincePotato = 0;
    Direction m_Heading = Direction::RIGHT;
    RingBuffer<Cell, BOARD_CELLS> m_Snake;
    OccupancySet<BOARD_CELLS> m_Occupied;
    Cell m_Food;
    std::optional<Bonus> m_Bonus;
    Scoring m_Scoring;
    InputBuffer m_Input;
    Random m_Random{0};
    std::uint32_t m_Ticks = 0;
    bool m_Running = true;
    bool m_Won = false;
};

} // namespace snake::sim
