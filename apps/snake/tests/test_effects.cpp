// SPDX-License-Identifier: GPL-3.0-or-later
#include "board/board_scene.hpp"
#include "board/effects.hpp"
#include "snake/sim/cell.hpp"
#include "snake/sim/direction.hpp"
#include "snake/sim/simulation.hpp"
#include "theme/tokens.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QPoint>
#include <QRect>
#include <QRgb>
#include <QSize>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

using snake::sim::Cell;
using snake::sim::Simulation;
using snake::ui::effects::Effects;
namespace board_scene = snake::ui::board_scene;
namespace tokens = snake::ui::tokens;

namespace
{

constexpr QRgb opaque(std::uint32_t rgb)
{
    return 0xFF000000U | rgb;
}

/// A snake of three pieces, head first.
std::vector<Cell> snake_of(std::size_t length = 3)
{
    std::vector<Cell> snake;
    for (std::size_t i = 0; i < length; ++i)
    {
        snake.push_back(Cell{static_cast<std::uint8_t>(10 - i), 10});
    }
    return snake;
}

/// Runs `effects` on in steps of a sixtieth of a second for `seconds`.
void run(Effects& effects, double seconds)
{
    for (double elapsed = 0.0; elapsed < seconds; elapsed += 1.0 / 60.0)
    {
        effects.advance(1.0 / 60.0);
    }
}

} // namespace

TEST_CASE("Nothing happens until something does", "[effects]")
{
    Effects effects;
    CHECK_FALSE(effects.active());
    CHECK(effects.particles().empty());
    CHECK(effects.scores().empty());
    CHECK(effects.shake() == QPoint());
    effects.advance(1.0);
    CHECK_FALSE(effects.active());
}

TEST_CASE("Eating throws a burst of specks from the middle of the cell", "[effects]")
{
    Effects effects;
    effects.eaten(Cell{4, 7}, 10, false);
    CHECK(effects.active());
    REQUIRE(effects.particles().size() == 8);
    for (const auto& speck : effects.particles())
    {
        CHECK(speck.x == 4.5);
        CHECK(speck.y == 7.5);
        CHECK(speck.age == 0.0);
    }
    // They go every way: the throws are not all in one half of the circle.
    const auto left =
        std::ranges::count_if(effects.particles(), [](const auto& speck) { return speck.velocity_x < 0; });
    CHECK(left >= 2);
    CHECK(left <= 6);
}

TEST_CASE("A bonus item gives a bigger burst in gold", "[effects]")
{
    Effects effects;
    effects.eaten(Cell{4, 7}, 50, true);
    CHECK(effects.particles().size() == 14);
    const auto gold = std::ranges::count_if(effects.particles(), [](const auto& speck)
                                            { return speck.colour == opaque(tokens::MEDAL_GOLD); });
    CHECK(gold >= 4);
}

TEST_CASE("The specks move, fall and are gone after their life", "[effects]")
{
    Effects effects;
    effects.eaten(Cell{10, 10}, 10, false);
    const auto before = effects.particles().front();
    effects.advance(0.1);
    const auto after = effects.particles().front();
    CHECK(after.x != before.x);
    CHECK(after.age == 0.1);
    // Gravity pulls down: the vertical speed grows by 2 cells a second in 0.1 s.
    CHECK(after.velocity_y > before.velocity_y);

    run(effects, Effects::PARTICLE_LIFE + 0.05);
    CHECK(effects.particles().empty());
}

TEST_CASE("There are never more specks than the cap", "[effects]")
{
    Effects effects;
    for (int i = 0; i < 40; ++i)
    {
        effects.eaten(Cell{3, 3}, 10, true);
    }
    CHECK(effects.particles().size() == Effects::MAX_PARTICLES);
}

TEST_CASE("The points rise from the cell and are gone after their life", "[effects]")
{
    Effects effects;
    effects.eaten(Cell{5, 5}, 30, false);
    REQUIRE(effects.scores().size() == 1);
    CHECK(effects.scores().front().points == 30);
    CHECK(Effects::score_progress(effects.scores().front()) == 0.0);
    effects.advance(Effects::SCORE_LIFE / 2);
    CHECK(Effects::score_progress(effects.scores().front()) > 0.49);
    CHECK(Effects::score_progress(effects.scores().front()) < 0.51);
    run(effects, Effects::SCORE_LIFE);
    CHECK(effects.scores().empty());
    CHECK_FALSE(effects.active());
}

TEST_CASE("A food worth nothing shows no points", "[effects]")
{
    Effects effects;
    effects.eaten(Cell{5, 5}, 0, false);
    CHECK(effects.scores().empty());
    CHECK_FALSE(effects.particles().empty());
}

TEST_CASE("A crash shakes the board and the shake dies away", "[effects]")
{
    Effects effects;
    effects.crashed(snake_of());
    CHECK(effects.active());
    bool moved = false;
    int largest = 0;
    for (double elapsed = 0.0; elapsed < Effects::SHAKE_LIFE; elapsed += 1.0 / 120.0)
    {
        const QPoint shake = effects.shake();
        moved = moved || shake != QPoint();
        largest = std::max({largest, std::abs(shake.x()), std::abs(shake.y())});
        effects.advance(1.0 / 120.0);
    }
    CHECK(moved);
    // Subtle: never more than the three pixels of the design.
    CHECK(largest <= 3);
    CHECK(effects.shake() == QPoint());
    // The snake is still dying, which takes longer than the shake.
    CHECK(effects.dying());
    run(effects, 2.0);
    CHECK_FALSE(effects.active());
}

TEST_CASE("With reduced motion a crash does not shake, but the burst still shows", "[effects]")
{
    Effects effects;
    effects.set_reduced_motion(true);
    CHECK(effects.reduced_motion());
    effects.crashed(snake_of());
    for (int i = 0; i < 30; ++i)
    {
        CHECK(effects.shake() == QPoint());
        effects.advance(0.01);
    }
    effects.eaten(Cell{1, 1}, 10, false);
    CHECK_FALSE(effects.particles().empty());
}

TEST_CASE("Turning reduced motion on stops a shake that is under way", "[effects]")
{
    Effects effects;
    effects.crashed(snake_of());
    effects.advance(0.01);
    effects.set_reduced_motion(true);
    CHECK(effects.shake() == QPoint());
    effects.set_reduced_motion(false);
    effects.crashed(snake_of());
    effects.advance(0.01);
    CHECK(effects.shake() != QPoint());
}

TEST_CASE("Clearing drops everything", "[effects]")
{
    Effects effects;
    effects.eaten(Cell{1, 1}, 10, false);
    effects.crashed(snake_of());
    effects.clear();
    CHECK_FALSE(effects.active());
    CHECK_FALSE(effects.dying());
    CHECK(effects.death_look() == snake::ui::effects::DeathLook{});
    CHECK(effects.particles().empty());
    CHECK(effects.scores().empty());
}

TEST_CASE("A time that is not above zero does nothing", "[effects]")
{
    Effects effects;
    effects.eaten(Cell{1, 1}, 10, false);
    const auto before = effects.particles().front();
    effects.advance(0.0);
    effects.advance(-1.0);
    CHECK(effects.particles().front().x == before.x);
    CHECK(effects.particles().front().age == 0.0);
}

TEST_CASE("Two runs of the same events look the same", "[effects]")
{
    Effects first;
    Effects second;
    for (Effects* effects : {&first, &second})
    {
        effects->eaten(Cell{2, 3}, 10, false);
        effects->advance(0.2);
        effects->eaten(Cell{8, 9}, 50, true);
        effects->advance(0.1);
    }
    REQUIRE(first.particles().size() == second.particles().size());
    for (std::size_t i = 0; i < first.particles().size(); ++i)
    {
        CHECK(first.particles()[i].x == second.particles()[i].x);
        CHECK(first.particles()[i].y == second.particles()[i].y);
        CHECK(first.particles()[i].colour == second.particles()[i].colour);
    }
}

TEST_CASE("The pixel font has the digits and the plus, five rows of three", "[effects][glyphs]")
{
    // The number of lit pixels in each glyph, as the font has them.
    constexpr int LIT[] = {12, 8, 11, 11, 9, 11, 12, 7, 13, 12};
    for (int digit = 0; digit < 10; ++digit)
    {
        INFO("digit " << digit);
        const auto pixels = board_scene::glyph_pixels(static_cast<char>('0' + digit));
        CHECK(static_cast<int>(pixels.size()) == LIT[digit]);
        for (const QPoint pixel : pixels)
        {
            CHECK(pixel.x() >= 0);
            CHECK(pixel.x() < 3);
            CHECK(pixel.y() >= 0);
            CHECK(pixel.y() < 5);
        }
    }
    CHECK(board_scene::glyph_pixels('+').size() == 5);
    CHECK(board_scene::glyph_pixels('x').empty());
}

TEST_CASE("Each digit is different from every other", "[effects][glyphs]")
{
    for (char first = '0'; first <= '9'; ++first)
    {
        for (char second = static_cast<char>(first + 1); second <= '9'; ++second)
        {
            INFO(first << " and " << second);
            CHECK(board_scene::glyph_pixels(first) != board_scene::glyph_pixels(second));
        }
    }
}

TEST_CASE("The effect shapes are the specks and the points on the board", "[effects][scene]")
{
    const board_scene::Layout layout = board_scene::layout_for(QSize(400, 400));
    Effects effects;
    CHECK(board_scene::effect_shapes(layout, effects).empty());
    CHECK(board_scene::effect_shapes(board_scene::Layout{}, effects).empty());

    effects.eaten(Cell{10, 10}, 10, false);
    const auto shapes = board_scene::effect_shapes(layout, effects);
    // 8 specks, then the points "+10": a shadow and a copy of 5 + 8 + 12 lit pixels.
    CHECK(shapes.size() == 8 + 2 * (5 + 8 + 12));
    const QRect cells = layout.cells();
    for (const auto& shape : shapes)
    {
        CHECK(cells.contains(shape.rect));
    }
    // The specks begin in the middle of the cell, in the colours of the food.
    const QPoint middle(layout.origin.x() + (10 * layout.cell) + (layout.cell / 2),
                        layout.origin.y() + (10 * layout.cell) + (layout.cell / 2));
    CHECK(std::abs(shapes.front().rect.center().x() - middle.x()) <= 2);
    CHECK(std::abs(shapes.front().rect.center().y() - middle.y()) <= 2);
    // The digits are cream on an ink shadow.
    CHECK(shapes[8].colour == opaque(tokens::INK));
    CHECK(shapes.back().colour == opaque(tokens::SURFACE));
}

TEST_CASE("The points rise as they age", "[effects][scene]")
{
    const board_scene::Layout layout = board_scene::layout_for(QSize(400, 400));
    Effects effects;
    effects.eaten(Cell{10, 10}, 50, false);
    run(effects, Effects::PARTICLE_LIFE + 0.05);
    REQUIRE(effects.particles().empty());
    const int start = board_scene::effect_shapes(layout, effects).front().rect.top();
    effects.advance(0.2);
    const int later = board_scene::effect_shapes(layout, effects).front().rect.top();
    CHECK(later < start);
}

TEST_CASE("The points of a food at the edge stay on the board", "[effects][scene]")
{
    const board_scene::Layout layout = board_scene::layout_for(QSize(400, 400));
    const QRect cells = layout.cells();
    for (const Cell cell : {Cell{0, 0}, Cell{19, 0}, Cell{0, 19}, Cell{19, 19}})
    {
        Effects effects;
        effects.eaten(cell, 1000, false);
        run(effects, Effects::PARTICLE_LIFE + 0.05);
        for (const double age : {0.0, 0.3, 0.7})
        {
            Effects fresh;
            fresh.eaten(cell, 1000, false);
            run(fresh, Effects::PARTICLE_LIFE + 0.05);
            fresh.advance(age);
            for (const auto& shape : board_scene::effect_shapes(layout, fresh))
            {
                INFO("cell " << static_cast<int>(cell.x) << "," << static_cast<int>(cell.y) << " age " << age);
                CHECK(cells.contains(shape.rect));
            }
        }
    }
}

TEST_CASE("A speck outside the board is not drawn", "[effects][scene]")
{
    const board_scene::Layout layout = board_scene::layout_for(QSize(400, 400));
    Effects effects;
    effects.eaten(Cell{0, 0}, 0, false);
    // Some specks fly off the corner. Whatever is drawn is inside.
    for (int i = 0; i < 20; ++i)
    {
        effects.advance(0.02);
        for (const auto& shape : board_scene::effect_shapes(layout, effects))
        {
            CHECK(layout.cells().contains(shape.rect));
        }
    }
}

TEST_CASE("A snake that crashes flashes twice and then loses its pieces from the tail, keeping its head",
          "[effects][death]")
{
    Effects effects;
    CHECK_FALSE(effects.dying());
    CHECK(effects.death_look() == snake::ui::effects::DeathLook{});

    effects.crashed(snake_of(4));
    CHECK(effects.dying());
    // Lit for the first half of each cycle of 0.4 seconds, for 0.6 seconds.
    CHECK(effects.death_look().flash);
    CHECK(effects.death_look().popped == 0);
    effects.advance(0.25);
    CHECK_FALSE(effects.death_look().flash);
    effects.advance(0.2);
    CHECK(effects.death_look().flash);
    effects.advance(0.16);
    // The flashing is over at 0.6 and the first piece pops as it ends.
    CHECK_FALSE(effects.death_look().flash);
    CHECK(effects.death_look().popped == 1);
    effects.advance(0.05);
    CHECK(effects.death_look().popped == 2);
    effects.advance(0.05);
    CHECK(effects.death_look().popped == 3);
    // A beat after the last piece, and it is over.
    CHECK(effects.dying());
    effects.advance(0.05);
    CHECK_FALSE(effects.dying());
    // The head stays: three pieces of four have popped, and it goes on looking like that.
    effects.advance(5.0);
    CHECK(effects.death_look().popped == 3);
    effects.clear();
    CHECK(effects.death_look() == snake::ui::effects::DeathLook{});
}

TEST_CASE("Each piece that pops puffs out specks from where it was", "[effects][death]")
{
    Effects effects;
    effects.crashed(snake_of(3));
    CHECK(effects.particles().empty());
    effects.advance(0.59);
    CHECK(effects.particles().empty());
    effects.advance(0.02);
    // The tail popped: three specks, at the cell of the tail (8, 10).
    REQUIRE(effects.particles().size() == 3);
    CHECK(effects.particles().front().x == 8.5);
    CHECK(effects.particles().front().y == 10.5);
    effects.advance(0.05);
    CHECK(effects.particles().size() == 6);
}

TEST_CASE("A long snake does not take long to die", "[effects][death]")
{
    Effects effects;
    effects.crashed(snake_of(10));
    double time = 0.0;
    while (effects.dying() && time < 10.0)
    {
        effects.advance(0.01);
        time += 0.01;
    }
    CHECK(time < Effects::FLASH_END + (9 * Effects::POP_STEP) + 0.05);

    Effects long_one;
    std::vector<Cell> body;
    for (std::size_t i = 0; i < 200; ++i)
    {
        body.push_back(Cell{static_cast<std::uint8_t>(i % 20), static_cast<std::uint8_t>(i / 20)});
    }
    long_one.crashed(body);
    time = 0.0;
    while (long_one.dying() && time < 10.0)
    {
        long_one.advance(0.01);
        time += 0.01;
    }
    // The pieces pop faster, so that the pops take about a second whatever the length.
    CHECK(time < Effects::FLASH_END + Effects::MAX_POP_TIME + 0.1);
    CHECK(long_one.death_look().popped == 199);
}

TEST_CASE("With reduced motion the snake does not flash, but its pieces still pop", "[effects][death]")
{
    Effects effects;
    effects.set_reduced_motion(true);
    effects.crashed(snake_of(3));
    CHECK(effects.dying());
    for (int i = 0; i < 40; ++i)
    {
        CHECK_FALSE(effects.death_look().flash);
        effects.advance(0.05);
    }
    CHECK_FALSE(effects.dying());
    CHECK(effects.death_look().popped == 2);

    // The first piece pops at once and the whole death is over in a tenth of a second.
    Effects quick;
    quick.set_reduced_motion(true);
    quick.crashed(snake_of(3));
    quick.advance(0.001);
    CHECK(quick.death_look().popped == 1);
    quick.advance(0.1);
    CHECK_FALSE(quick.dying());
}

TEST_CASE("The death can be skipped", "[effects][death]")
{
    Effects effects;
    effects.crashed(snake_of(5));
    effects.skip_death();
    CHECK_FALSE(effects.dying());
    CHECK(effects.death_look().popped == 4);
    CHECK_FALSE(effects.death_look().flash);
    // Skipping when nothing is dying does nothing.
    Effects calm;
    calm.skip_death();
    CHECK(calm.death_look() == snake::ui::effects::DeathLook{});
}

TEST_CASE("A crash with no snake, or a snake of one piece, has nothing to pop", "[effects][death]")
{
    Effects empty;
    empty.crashed({});
    CHECK_FALSE(empty.dying());
    CHECK(empty.death_look() == snake::ui::effects::DeathLook{});

    Effects lone;
    lone.crashed(snake_of(1));
    CHECK(lone.dying());
    lone.advance(1.0);
    CHECK_FALSE(lone.dying());
    CHECK(lone.death_look().popped == 0);
}

TEST_CASE("The pieces that have popped are not drawn, and in a flash the pieces are white", "[effects][death][scene]")
{
    const board_scene::Layout layout = board_scene::layout_for(QSize(672, 672));
    Simulation::Setup setup;
    const std::vector<Cell> snake = {Cell{10, 10}, Cell{9, 10}, Cell{8, 10}, Cell{7, 10}};
    setup.heading = snake::sim::Direction::RIGHT;
    setup.snake = snake;
    setup.food = Cell{3, 3};
    setup.running = false;
    const auto over = Simulation::from_setup(setup);
    REQUIRE(over.has_value());

    const auto whole = board_scene::game_shapes(layout, *over, 0.0);
    const auto popped = board_scene::game_shapes(layout, *over, 0.0, 0.0, {.flash = false, .popped = 2});
    const auto head_only = board_scene::game_shapes(layout, *over, 0.0, 0.0, {.flash = false, .popped = 99});
    CHECK(popped.size() < whole.size());
    CHECK(head_only.size() < popped.size());
    // The tail is gone: nothing is drawn where it was.
    const QRect tail(layout.origin.x() + (7 * layout.cell), layout.origin.y() + (10 * layout.cell), layout.cell,
                     layout.cell);
    const auto drawn_on_tail = [&](const std::vector<board_scene::Shape>& shapes)
    {
        return std::ranges::any_of(shapes, [&](const board_scene::Shape& shape)
                                   { return tail.contains(shape.rect) && shape.colour != 0; });
    };
    CHECK(drawn_on_tail(whole));
    CHECK_FALSE(drawn_on_tail(popped));

    const auto flashing = board_scene::game_shapes(layout, *over, 0.0, 0.0, {.flash = true, .popped = 0});
    const auto white = static_cast<QRgb>(0xFF000000U | tokens::EYE);
    const auto whites = [&](const std::vector<board_scene::Shape>& shapes)
    { return std::ranges::count_if(shapes, [&](const board_scene::Shape& shape) { return shape.colour == white; }); };
    CHECK(whites(flashing) > whites(whole));
}

TEST_CASE("A snake that crashed has crosses for eyes, and one that won or is running has pupils",
          "[effects][death][scene]")
{
    const board_scene::Layout layout = board_scene::layout_for(QSize(672, 672));
    const std::vector<Cell> snake = {Cell{10, 10}, Cell{9, 10}, Cell{8, 10}};
    Simulation::Setup setup;
    setup.snake = snake;
    setup.food = Cell{3, 3};

    const auto ink = static_cast<QRgb>(0xFF000000U | tokens::INK);
    const auto inks = [&](const Simulation& game)
    {
        const auto shapes = board_scene::game_shapes(layout, game, 0.0);
        return std::ranges::count_if(shapes, [&](const board_scene::Shape& shape) { return shape.colour == ink; });
    };

    const auto running = Simulation::from_setup(setup);
    REQUIRE(running.has_value());
    const auto pupils = inks(*running);

    setup.running = false;
    const auto crashed = Simulation::from_setup(setup);
    REQUIRE(crashed.has_value());
    // Each eye has five squares of a cross in place of one pupil.
    CHECK(inks(*crashed) == pupils + (2 * 4));
}
