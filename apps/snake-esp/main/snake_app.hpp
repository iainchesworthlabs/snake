// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "lvgl.h"
#include "snake/input/joystick_steering.hpp"
#include "snake/raster/board_painter.hpp"
#include "snake/sim/mode.hpp"
#include "snake/sim/session.hpp"
#include "snake/sim/tick_pacer.hpp"

#include <cstdint>

namespace snake::esp
{

/// The game on the ESP32-P4's panel: a title with the four modes, the board with the score beside it, and the end of
/// the game. It is drawn with LVGL (the board is a canvas that libs/raster paints into) and steered by the thumb: an
/// on-screen joystick, a d-pad or a swipe, as the phones are, with the joystick to begin with. Tilt is listed and off:
/// the board has no motion sensor. Every call is made with LVGL's lock held: the constructor by app_main, the rest by
/// LVGL's own task.
///
/// Not run on a board yet (roadmap step 23): it is built, and the parts that do not need the chip are tested on the
/// desktop.
class SnakeApp
{
public:
    /// The size of a cell of the board on the 1024 by 600 panel: the board is 552 pixels square.
    static constexpr int CELL_PIXELS = 27;

    /// The size of the chips that choose how to steer.
    static constexpr int CHIP_WIDTH = 90;
    static constexpr int CHIP_HEIGHT = 44;

    /// What steers the snake. Tilt is not one of them until the board has a sensor.
    enum class Controls : std::uint8_t
    {
        JOYSTICK,
        DPAD,
        SWIPE,
    };

    /// Builds the screens on the active LVGL screen and starts the clock that runs the game. The app lives for as long
    /// as the firmware does.
    SnakeApp();

    SnakeApp(const SnakeApp&) = delete;
    SnakeApp& operator=(const SnakeApp&) = delete;
    SnakeApp(SnakeApp&&) = delete;
    SnakeApp& operator=(SnakeApp&&) = delete;
    ~SnakeApp() = default;

private:
    void build_title(lv_obj_t* parent);
    void build_game(lv_obj_t* parent);
    void build_over(lv_obj_t* parent);
    /// The right-hand column of the game: the joystick, the d-pad, the chips that choose between them and the tilt
    /// chip.
    void build_controls(lv_obj_t* parent);
    lv_obj_t* make_chip(lv_obj_t* parent, const char* text, int x, int y, lv_event_cb_t callback);

    void start_game(snake::sim::Mode mode);
    void show_title();
    void show_over();
    /// Runs the ticks that are due and redraws when something changed. Called by an LVGL timer.
    void on_timer();
    /// Draws the board and the numbers beside it.
    void refresh();
    void steer(lv_dir_t direction);
    void turn(snake::sim::Direction direction);
    void set_controls(Controls controls);
    /// The thumb is at (dx, dy) pixels from the middle of the joystick.
    void follow_stick(int dx, int dy);
    void let_go_of_stick();

    static void on_timer_trampoline(lv_timer_t* timer);
    static void on_mode_clicked(lv_event_t* event);
    static void on_gesture(lv_event_t* event);
    static void on_stick(lv_event_t* event);
    static void on_pad(lv_event_t* event);
    static void on_controls_chip(lv_event_t* event);
    static void on_shed_clicked(lv_event_t* event);
    static void on_menu_clicked(lv_event_t* event);
    static void on_exit_clicked(lv_event_t* event);
    static void on_again_clicked(lv_event_t* event);

    snake::sim::Session m_Session;
    snake::sim::TickPacer m_Pacer;
    snake::sim::Mode m_Mode = snake::sim::Mode::EASY;
    std::uint32_t m_LastTickMs = 0;
    std::uint32_t m_Frame = 0;
    Controls m_Controls = Controls::JOYSTICK;
    snake::input::JoystickSteering m_Stick;

    lv_obj_t* m_Title = nullptr;
    lv_obj_t* m_Game = nullptr;
    lv_obj_t* m_Over = nullptr;
    lv_obj_t* m_Canvas = nullptr;
    lv_obj_t* m_ScoreLabel = nullptr;
    lv_obj_t* m_ModeLabel = nullptr;
    lv_obj_t* m_ShedButton = nullptr;
    lv_obj_t* m_Fuse = nullptr;
    lv_obj_t* m_OverLabel = nullptr;
    lv_obj_t* m_StickPlate = nullptr;
    lv_obj_t* m_StickKnob = nullptr;
    lv_obj_t* m_Pad = nullptr;
    lv_obj_t* m_StickChip = nullptr;
    lv_obj_t* m_PadChip = nullptr;
    lv_obj_t* m_SwipeChip = nullptr;
    lv_obj_t* m_SwipeNote = nullptr;
    std::uint16_t* m_Pixels = nullptr;
};

} // namespace snake::esp
