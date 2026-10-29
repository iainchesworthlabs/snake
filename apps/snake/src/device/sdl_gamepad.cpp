// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/sdl_gamepad.hpp"

#include "device/gamepad.hpp"
#include "snake/input/gamepad_map.hpp"

#include <SDL3/SDL.h>

#include <QDebug>
#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>

namespace snake::ui::device
{

namespace
{

std::optional<input::GamepadButton> button_from_sdl(std::uint8_t button) noexcept
{
    switch (button)
    {
    case SDL_GAMEPAD_BUTTON_DPAD_UP:
        return input::GamepadButton::DPAD_UP;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
        return input::GamepadButton::DPAD_DOWN;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
        return input::GamepadButton::DPAD_LEFT;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
        return input::GamepadButton::DPAD_RIGHT;
    case SDL_GAMEPAD_BUTTON_SOUTH:
        return input::GamepadButton::SOUTH;
    case SDL_GAMEPAD_BUTTON_EAST:
        return input::GamepadButton::EAST;
    case SDL_GAMEPAD_BUTTON_START:
        return input::GamepadButton::START;
    case SDL_GAMEPAD_BUTTON_BACK:
        return input::GamepadButton::SELECT;
    default:
        return std::nullopt;
    }
}

/// A stick axis of SDL (-32768 to 32767, with down and right positive) as -1 to 1.
float unit_of(std::int16_t value) noexcept
{
    return std::clamp(static_cast<float>(value) / 32767.0F, -1.0F, 1.0F);
}

class SdlGamepads final : public GamepadSource
{
public:
    /// Starts SDL's gamepad subsystem. `ok()` is false when it would not start.
    SdlGamepads()
    {
        // The game window is Qt's, not SDL's, so SDL does not know it has the focus: read the pads all the same. The
        // game stops listening itself when its window is not the active one.
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
        m_Started = SDL_InitSubSystem(SDL_INIT_GAMEPAD);
        if (!m_Started)
        {
            qWarning() << "Gamepads could not be started:" << SDL_GetError();
        }
    }

    ~SdlGamepads() override
    {
        for (const auto& [id, pad] : m_Pads)
        {
            SDL_CloseGamepad(pad);
        }
        if (m_Started)
        {
            SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
        }
    }

    [[nodiscard]] bool ok() const noexcept
    {
        return m_Started;
    }

    void poll(const Handler& handler) override
    {
        SDL_UpdateGamepads();
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_EVENT_GAMEPAD_ADDED:
                add(event.gdevice.which, handler);
                break;
            case SDL_EVENT_GAMEPAD_REMOVED:
                remove(event.gdevice.which, handler);
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
                if (const auto button = button_from_sdl(event.gbutton.button))
                {
                    handler({.kind = event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN ? GamepadEvent::Kind::BUTTON_DOWN
                                                                                 : GamepadEvent::Kind::BUTTON_UP,
                             .button = *button});
                }
                break;
            case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                move_stick(event.gaxis, handler);
                break;
            default:
                break;
            }
        }
    }

private:
    void add(SDL_JoystickID id, const Handler& handler)
    {
        if (m_Pads.contains(id))
        {
            return;
        }
        if (SDL_Gamepad* pad = SDL_OpenGamepad(id))
        {
            m_Pads.emplace(id, pad);
            handler({.kind = GamepadEvent::Kind::CONNECTED});
        }
    }

    void remove(SDL_JoystickID id, const Handler& handler)
    {
        const auto found = m_Pads.find(id);
        if (found == m_Pads.end())
        {
            return;
        }
        SDL_CloseGamepad(found->second);
        m_Pads.erase(found);
        if (m_Pads.empty())
        {
            m_StickX = 0.0F;
            m_StickY = 0.0F;
            handler({.kind = GamepadEvent::Kind::STICK});
        }
        handler({.kind = GamepadEvent::Kind::DISCONNECTED});
    }

    void move_stick(const SDL_GamepadAxisEvent& axis, const Handler& handler)
    {
        if (axis.axis == SDL_GAMEPAD_AXIS_LEFTX)
        {
            m_StickX = unit_of(axis.value);
        }
        else if (axis.axis == SDL_GAMEPAD_AXIS_LEFTY)
        {
            m_StickY = unit_of(axis.value);
        }
        else
        {
            return;
        }
        // SDL reports down as positive; the game's up is positive.
        handler({.kind = GamepadEvent::Kind::STICK, .right = m_StickX, .up = -m_StickY});
    }

    bool m_Started = false;
    std::map<SDL_JoystickID, SDL_Gamepad*> m_Pads;
    float m_StickX = 0.0F;
    float m_StickY = 0.0F;
};

} // namespace

std::unique_ptr<GamepadSource> make_sdl_gamepad_source()
{
    auto pads = std::make_unique<SdlGamepads>();
    if (!pads->ok())
    {
        return nullptr;
    }
    return pads;
}

} // namespace snake::ui::device
