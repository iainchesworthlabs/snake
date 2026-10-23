// SPDX-License-Identifier: GPL-3.0-or-later
#include <string>

#include "snake/platform.hpp"

namespace snake::platform
{

// A browser has no host name to report.
std::string host_name()
{
    return "browser";
}

}  // namespace snake::platform
