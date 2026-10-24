// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/input/direction_selector.hpp"
#include "snake/sim/direction.hpp"

#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <optional>

using namespace snake::input;
using snake::sim::Direction;

namespace
{

// The tilt numbers: let go below 12, set at 18, switch axis when the other is 5 larger.
constexpr SelectorConfig CONFIG = {.release = 12.0F, .engage = 18.0F, .switch_margin = 5.0F};

} // namespace

TEST_CASE("Level input gives no direction", "[selector]")
{
    DirectionSelector selector(CONFIG);
    CHECK_FALSE(selector.update(0.0F, 0.0F));
    CHECK_FALSE(selector.update(11.0F, -11.0F));
    CHECK_FALSE(selector.current());
}

TEST_CASE("A value between release and engage does not set a direction", "[selector]")
{
    DirectionSelector selector(CONFIG);
    CHECK_FALSE(selector.update(15.0F, 0.0F));
    CHECK_FALSE(selector.update(0.0F, -17.9F));
}

TEST_CASE("A value at the engage point sets the direction of its axis", "[selector]")
{
    DirectionSelector selector(CONFIG);
    CHECK(selector.update(18.0F, 0.0F) == Direction::RIGHT);
    selector.reset();
    CHECK(selector.update(-18.0F, 0.0F) == Direction::LEFT);
    selector.reset();
    CHECK(selector.update(0.0F, 18.0F) == Direction::UP);
    selector.reset();
    CHECK(selector.update(0.0F, -18.0F) == Direction::DOWN);
}

TEST_CASE("When both axes are past the engage point the larger one wins", "[selector]")
{
    DirectionSelector selector(CONFIG);
    CHECK(selector.update(25.0F, 20.0F) == Direction::RIGHT);
    selector.reset();
    CHECK(selector.update(20.0F, -25.0F) == Direction::DOWN);
    selector.reset();
    CHECK(selector.update(-30.0F, 19.0F) == Direction::LEFT);
    selector.reset();
    CHECK(selector.update(-19.0F, 30.0F) == Direction::UP);
}

TEST_CASE("A tie between the axes goes to the horizontal one", "[selector]")
{
    DirectionSelector selector(CONFIG);
    CHECK(selector.update(-20.0F, 20.0F) == Direction::LEFT);
}

TEST_CASE("A direction stays while its axis is between release and engage", "[selector]")
{
    DirectionSelector selector(CONFIG);
    REQUIRE(selector.update(20.0F, 0.0F) == Direction::RIGHT);
    CHECK(selector.update(15.0F, 0.0F) == Direction::RIGHT);
    CHECK(selector.update(12.0F, 0.0F) == Direction::RIGHT);
    CHECK(selector.current() == Direction::RIGHT);
}

TEST_CASE("A direction lets go when its axis falls below release", "[selector]")
{
    DirectionSelector selector(CONFIG);
    REQUIRE(selector.update(20.0F, 0.0F) == Direction::RIGHT);
    CHECK_FALSE(selector.update(11.9F, 0.0F));
    CHECK_FALSE(selector.current());
    // It needs the engage point again, not just the release point.
    CHECK_FALSE(selector.update(15.0F, 0.0F));
    CHECK(selector.update(18.0F, 0.0F) == Direction::RIGHT);
}

TEST_CASE("Tilting through level to the other side sets the opposite direction", "[selector]")
{
    DirectionSelector selector(CONFIG);
    REQUIRE(selector.update(20.0F, 0.0F) == Direction::RIGHT);
    CHECK(selector.update(-20.0F, 0.0F) == Direction::LEFT);
    CHECK(selector.update(0.0F, 20.0F) == Direction::UP);
    CHECK(selector.update(0.0F, -20.0F) == Direction::DOWN);
}

TEST_CASE("A set direction whose axis has gone to the other side lets go even inside the band", "[selector]")
{
    DirectionSelector selector(CONFIG);
    REQUIRE(selector.update(20.0F, 0.0F) == Direction::RIGHT);
    CHECK_FALSE(selector.update(-15.0F, 0.0F));
}

TEST_CASE("The axis in control keeps priority until the other is 5 larger", "[selector]")
{
    DirectionSelector selector(CONFIG);
    REQUIRE(selector.update(22.0F, 0.0F) == Direction::RIGHT);
    CHECK(selector.update(22.0F, 22.0F) == Direction::RIGHT);
    CHECK(selector.update(22.0F, 26.9F) == Direction::RIGHT);
    CHECK(selector.update(22.0F, 27.0F) == Direction::UP);
    // Now the vertical axis is in control and holds against the same margin.
    CHECK(selector.update(31.9F, 27.0F) == Direction::UP);
    CHECK(selector.update(32.0F, 27.0F) == Direction::RIGHT);
}

TEST_CASE("The other axis must also reach the engage point to take over", "[selector]")
{
    DirectionSelector selector(CONFIG);
    // The margin alone is not enough: 17 is 5 more than 12, but 17 is below the engage point.
    REQUIRE(selector.update(20.0F, 0.0F) == Direction::RIGHT);
    CHECK(selector.update(12.0F, 17.0F) == Direction::RIGHT);
    CHECK(selector.update(12.0F, 18.0F) == Direction::UP);
}

TEST_CASE("The other axis takes over towards its own side", "[selector]")
{
    DirectionSelector selector(CONFIG);
    REQUIRE(selector.update(20.0F, 0.0F) == Direction::RIGHT);
    CHECK(selector.update(20.0F, -30.0F) == Direction::DOWN);
    CHECK(selector.update(-40.0F, -30.0F) == Direction::LEFT);
}

TEST_CASE("A diagonal hold that wobbles so that the axes differ by less than the margin never flickers", "[selector]")
{
    DirectionSelector selector(CONFIG);
    REQUIRE(selector.update(24.0F, 20.0F) == Direction::RIGHT);
    const float wobble[] = {-2.4F, 2.0F, -1.0F, 2.4F, -2.4F, 1.5F, 0.0F, -2.0F, 2.4F, -2.4F};
    for (const float value : wobble)
    {
        CHECK(selector.update(24.0F + value, 24.0F - value) == Direction::RIGHT);
    }
}

TEST_CASE("Reset forgets the direction", "[selector]")
{
    DirectionSelector selector(CONFIG);
    REQUIRE(selector.update(20.0F, 0.0F));
    selector.reset();
    CHECK_FALSE(selector.current());
    // 15 would have kept the old direction; after a reset it sets nothing.
    CHECK_FALSE(selector.update(15.0F, 0.0F));
}

TEST_CASE("A value that is not a number sets nothing and lets go", "[selector]")
{
    DirectionSelector selector(CONFIG);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    CHECK_FALSE(selector.update(nan, nan));
    REQUIRE(selector.update(20.0F, 0.0F) == Direction::RIGHT);
    CHECK_FALSE(selector.update(nan, 0.0F));
}

TEST_CASE("The selector reports the numbers it was given", "[selector]")
{
    const DirectionSelector selector(CONFIG);
    CHECK(selector.config().release == 12.0F);
    CHECK(selector.config().engage == 18.0F);
    CHECK(selector.config().switch_margin == 5.0F);
}
