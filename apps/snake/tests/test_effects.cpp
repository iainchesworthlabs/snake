// SPDX-License-Identifier: GPL-3.0-or-later
#include "board/board_scene.hpp"
#include "board/effects.hpp"
#include "snake/sim/cell.hpp"
#include "theme/tokens.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QPoint>
#include <QRect>
#include <QRgb>
#include <QSize>
#include <algorithm>
#include <cstddef>
#include <cstdint>

using snake::sim::Cell;
using snake::ui::effects::Effects;
namespace board_scene = snake::ui::board_scene;
namespace tokens = snake::ui::tokens;

namespace
{

constexpr QRgb opaque(std::uint32_t rgb)
{
    return 0xFF000000U | rgb;
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
    effects.crashed();
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
    CHECK_FALSE(effects.active());
}

TEST_CASE("With reduced motion a crash does not shake, but the burst still shows", "[effects]")
{
    Effects effects;
    effects.set_reduced_motion(true);
    CHECK(effects.reduced_motion());
    effects.crashed();
    CHECK_FALSE(effects.active());
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
    effects.crashed();
    effects.advance(0.01);
    effects.set_reduced_motion(true);
    CHECK_FALSE(effects.active());
    CHECK(effects.shake() == QPoint());
    effects.set_reduced_motion(false);
    effects.crashed();
    CHECK(effects.active());
}

TEST_CASE("Clearing drops everything", "[effects]")
{
    Effects effects;
    effects.eaten(Cell{1, 1}, 10, false);
    effects.crashed();
    effects.clear();
    CHECK_FALSE(effects.active());
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
