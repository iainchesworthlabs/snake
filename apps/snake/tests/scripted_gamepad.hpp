// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "device/gamepad.hpp"

#include <vector>

namespace snake::test
{

/// A gamepad source that gives the events that are in `script` at the next poll, and empties it: for tests of what the
/// game does when a pad is used.
class ScriptedGamepad final : public ui::device::GamepadSource
{
public:
    explicit ScriptedGamepad(std::vector<ui::device::GamepadEvent>* script) : m_Script(script) {}

    void poll(const Handler& handler) override
    {
        std::vector<ui::device::GamepadEvent> events;
        events.swap(*m_Script);
        for (const ui::device::GamepadEvent& event : events)
        {
            handler(event);
        }
    }

private:
    std::vector<ui::device::GamepadEvent>* m_Script;
};

} // namespace snake::test
