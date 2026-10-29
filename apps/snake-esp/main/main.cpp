// SPDX-License-Identifier: GPL-3.0-or-later

// The entry point of the ESP32-P4 front end: starts the panel and the touch screen of the board through Espressif's
// board support package, and hands the screen to the game.
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "snake_app.hpp"

extern "C" void app_main()
{
    // The display, the touch controller and LVGL's task, with the backlight on.
    lv_display_t* display = bsp_display_start();
    if (display == nullptr)
    {
        ESP_LOGE("snake", "the display did not start");
        return;
    }
    bsp_display_backlight_on();

    // LVGL runs on a task of its own: everything that touches it is done with its lock held.
    bsp_display_lock(0);
    static snake::esp::SnakeApp app;
    bsp_display_unlock();
}
