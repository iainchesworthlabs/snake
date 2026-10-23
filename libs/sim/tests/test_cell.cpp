// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/sim/cell.hpp"

#include <catch2/catch_test_macros.hpp>

using snake::sim::BOARD_CELLS;
using snake::sim::BOARD_COLUMNS;
using snake::sim::BOARD_ROWS;
using snake::sim::Cell;

static_assert(snake::sim::to_index(Cell{3, 2}) == 43);
static_assert(snake::sim::from_index(43) == Cell{3, 2});

TEST_CASE("The board is 20 by 20", "[cell]")
{
    CHECK(BOARD_COLUMNS == 20);
    CHECK(BOARD_ROWS == 20);
    CHECK(BOARD_CELLS == 400);
}

TEST_CASE("Cells are numbered row by row from the top left", "[cell]")
{
    CHECK(snake::sim::to_index(Cell{0, 0}) == 0);
    CHECK(snake::sim::to_index(Cell{19, 0}) == 19);
    CHECK(snake::sim::to_index(Cell{0, 1}) == 20);
    CHECK(snake::sim::to_index(Cell{19, 19}) == 399);
}

TEST_CASE("Every index converts to a cell and back", "[cell]")
{
    for (std::uint16_t index = 0; index < BOARD_CELLS; ++index)
    {
        const Cell cell = snake::sim::from_index(index);
        REQUIRE(snake::sim::on_board(cell));
        REQUIRE(snake::sim::to_index(cell) == index);
    }
}

TEST_CASE("A cell is on the board only inside the 20 by 20 grid", "[cell]")
{
    CHECK(snake::sim::on_board(Cell{0, 0}));
    CHECK(snake::sim::on_board(Cell{19, 19}));
    CHECK_FALSE(snake::sim::on_board(Cell{20, 0}));
    CHECK_FALSE(snake::sim::on_board(Cell{0, 20}));
}

TEST_CASE("Cells compare by position", "[cell]")
{
    CHECK(Cell{4, 5} == Cell{4, 5});
    CHECK_FALSE(Cell{4, 5} == Cell{5, 4});
}
