// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform/touch_screen.hpp"

#include <QInputDevice>

namespace snake::ui
{

bool has_touch_screen()
{
    const auto devices = QInputDevice::devices();
    for (const QInputDevice* device : devices)
    {
        if (device->type() == QInputDevice::DeviceType::TouchScreen)
        {
            return true;
        }
    }
    return false;
}

} // namespace snake::ui
