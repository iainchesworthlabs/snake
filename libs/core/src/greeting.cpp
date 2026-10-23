// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/greeting.hpp"

#include <utility>

#include "snake/platform.hpp"

namespace snake
{

Greeter::Greeter(std::string name) : m_Name(std::move(name))
{
}

std::string Greeter::greet() const
{
    std::string text = "Hello, ";
    text += m_Name;
    text += ", from ";
    text += platform::host_name();
    text += ".";
    return text;
}

}  // namespace snake
