// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/input/gamepad_map.hpp"

#include <cstdint>
#include <functional>
#include <memory>

namespace snake::ui::device
{

/// Something a gamepad did, in the terms of libs/input: a button of the library's list went down or up, the left stick
/// is somewhere, or a pad came or went. Buttons the game does not use never reach it.
struct GamepadEvent
{
    enum class Kind : std::uint8_t
    {
        BUTTON_DOWN,
        BUTTON_UP,
        STICK,        // the left stick moved: `right` and `up`
        CONNECTED,    // a pad was plugged in or paired
        DISCONNECTED, // a pad was unplugged or went away
    };

    Kind kind = Kind::STICK;
    input::GamepadButton button = input::GamepadButton::SOUTH; // for BUTTON_DOWN and BUTTON_UP
    float right = 0.0F;                                        // for STICK: -1 to 1, right positive
    float up = 0.0F;                                           // for STICK: -1 to 1, up positive
};

/// The gamepads of the computer, read by asking. poll() gives every event since the last call, in the order they
/// happened.
class GamepadSource
{
public:
    GamepadSource() = default;
    GamepadSource(const GamepadSource&) = delete;
    GamepadSource& operator=(const GamepadSource&) = delete;
    GamepadSource(GamepadSource&&) = delete;
    GamepadSource& operator=(GamepadSource&&) = delete;
    virtual ~GamepadSource() = default;

    using Handler = std::function<void(const GamepadEvent&)>;
    virtual void poll(const Handler& handler) = 0;
};

/// The gamepads of this device, or nothing when it cannot read any (a phone, or a computer where the library that reads
/// them did not start). Defined once for each operating system, in src/device/<os>/.
[[nodiscard]] std::unique_ptr<GamepadSource> make_gamepad_source();

/// What makes the gamepad source: by default make_gamepad_source() above. Tests give a recording instead, and an empty
/// function puts the device's own back.
using GamepadFactory = std::function<std::unique_ptr<GamepadSource>()>;
void set_gamepad_factory(GamepadFactory factory);

/// The gamepad source the game reads: the factory's, or the device's own.
[[nodiscard]] std::unique_ptr<GamepadSource> gamepad_source();

} // namespace snake::ui::device
