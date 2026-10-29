// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/gamepad.hpp"
#include "device/sdl_gamepad.hpp"
#include "snake/input/gamepad_map.hpp"

#include <SDL3/SDL.h>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <thread>
#include <vector>

using snake::input::GamepadButton;
using snake::ui::device::GamepadEvent;
using Kind = GamepadEvent::Kind;

namespace
{

/// A gamepad that SDL makes up, so that the reading of one can be tried on a machine with none: the buttons and axes
/// are in the order of SDL's own list.
class VirtualPad
{
public:
    VirtualPad()
    {
        SDL_InitSubSystem(SDL_INIT_GAMEPAD);
        SDL_VirtualJoystickDesc desc;
        SDL_INIT_INTERFACE(&desc);
        desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
        desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
        desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
        desc.name = "Test pad";
        m_Id = SDL_AttachVirtualJoystick(&desc);
        m_Joystick = m_Id != 0 ? SDL_OpenJoystick(m_Id) : nullptr;
    }

    ~VirtualPad()
    {
        if (m_Joystick != nullptr)
        {
            SDL_CloseJoystick(m_Joystick);
        }
        if (m_Id != 0)
        {
            SDL_DetachVirtualJoystick(m_Id);
        }
        SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
    }

    VirtualPad(const VirtualPad&) = delete;
    VirtualPad& operator=(const VirtualPad&) = delete;
    VirtualPad(VirtualPad&&) = delete;
    VirtualPad& operator=(VirtualPad&&) = delete;

    [[nodiscard]] bool ok() const noexcept
    {
        return m_Joystick != nullptr;
    }

    void button(SDL_GamepadButton which, bool down) const
    {
        SDL_SetJoystickVirtualButton(m_Joystick, which, down);
    }

    void axis(SDL_GamepadAxis which, Sint16 value) const
    {
        SDL_SetJoystickVirtualAxis(m_Joystick, which, value);
    }

private:
    SDL_JoystickID m_Id = 0;
    SDL_Joystick* m_Joystick = nullptr;
};

std::vector<GamepadEvent> read(snake::ui::device::GamepadSource& source)
{
    std::vector<GamepadEvent> events;
    // SDL hands over a virtual pad's changes at the next update; a few rounds make sure of them.
    for (int round = 0; round < 3; ++round)
    {
        source.poll([&events](const GamepadEvent& event) { events.push_back(event); });
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return events;
}

} // namespace

TEST_CASE("SDL's gamepads are read as the game's own events", "[gamepad][sdl]")
{
    auto source = snake::ui::device::make_sdl_gamepad_source();
    REQUIRE(source != nullptr);

    // No pad yet, so nothing to say.
    CHECK(read(*source).empty());

    const VirtualPad pad;
    REQUIRE(pad.ok());
    const std::vector<GamepadEvent> added = read(*source);
    REQUIRE_FALSE(added.empty());
    CHECK(added.front().kind == Kind::CONNECTED);

    pad.button(SDL_GAMEPAD_BUTTON_SOUTH, true);
    pad.button(SDL_GAMEPAD_BUTTON_START, true);
    pad.button(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, true); // the game has no use for it
    pad.button(SDL_GAMEPAD_BUTTON_DPAD_UP, true);
    const std::vector<GamepadEvent> pressed = read(*source);
    REQUIRE(pressed.size() == 3);
    CHECK(pressed[0].kind == Kind::BUTTON_DOWN);
    CHECK(pressed[0].button == GamepadButton::SOUTH);
    CHECK(pressed[1].button == GamepadButton::START);
    CHECK(pressed[2].button == GamepadButton::DPAD_UP);

    pad.button(SDL_GAMEPAD_BUTTON_SOUTH, false);
    const std::vector<GamepadEvent> released = read(*source);
    REQUIRE(released.size() == 1);
    CHECK(released[0].kind == Kind::BUTTON_UP);
    CHECK(released[0].button == GamepadButton::SOUTH);

    // Right and down on SDL's stick are positive; the game's up is positive.
    pad.axis(SDL_GAMEPAD_AXIS_LEFTX, 32767);
    pad.axis(SDL_GAMEPAD_AXIS_LEFTY, -32768);
    const std::vector<GamepadEvent> pushed = read(*source);
    REQUIRE_FALSE(pushed.empty());
    CHECK(pushed.back().kind == Kind::STICK);
    CHECK(pushed.back().right == 1.0F);
    CHECK(pushed.back().up == 1.0F);
}

TEST_CASE("A pad that is unplugged is reported, and the stick is let go", "[gamepad][sdl]")
{
    auto source = snake::ui::device::make_sdl_gamepad_source();
    REQUIRE(source != nullptr);
    {
        const VirtualPad pad;
        REQUIRE(pad.ok());
        static_cast<void>(read(*source));
        pad.axis(SDL_GAMEPAD_AXIS_LEFTX, 32767);
        static_cast<void>(read(*source));
    }
    const std::vector<GamepadEvent> gone = read(*source);
    REQUIRE_FALSE(gone.empty());
    CHECK(gone.back().kind == Kind::DISCONNECTED);
    bool let_go = false;
    for (const GamepadEvent& event : gone)
    {
        if (event.kind == Kind::STICK && event.right == 0.0F && event.up == 0.0F)
        {
            let_go = true;
        }
    }
    CHECK(let_go);
}
