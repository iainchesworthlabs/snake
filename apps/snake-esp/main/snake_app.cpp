// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake_app.hpp"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_random.h"
#include "lvgl.h"
#include "snake/raster/board_painter.hpp"
#include "snake/raster/palette.hpp"
#include "snake/sim/direction.hpp"
#include "snake/sim/mode.hpp"
#include "snake/sim/session.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace snake::esp
{

namespace
{

constexpr const char* TAG = "snake";

// The panel, and the two columns beside the board: the score on the left, the controls on the right, each as wide as
// what is left of the panel after the board and a margin on every side.
constexpr int PANEL_WIDTH = 1024;
constexpr int PANEL_HEIGHT = 600;
constexpr int MARGIN = 24;

// The joystick of the phones: a cream plate, and a coral knob that can travel from the middle to the edge of the plate.
constexpr int STICK_PLATE = 176;
constexpr int STICK_KNOB = 72;
constexpr int STICK_TRAVEL = (STICK_PLATE - STICK_KNOB) / 2;
constexpr int STICK_TOP = 100;
constexpr int PAD_KEY = 56;

// The names of the modes, in the order of the title's buttons. The translations come with the pixel fonts (roadmap step
// 22): the built-in fonts of LVGL have Latin letters only.
struct ModeButton
{
    snake::sim::Mode mode;
    const char* name;
};
constexpr std::array<ModeButton, 4> MODES = {{{snake::sim::Mode::EASY, "Easy"},
                                              {snake::sim::Mode::CLASSIC, "Classic"},
                                              {snake::sim::Mode::PORTALS, "Portals"},
                                              {snake::sim::Mode::HOT_POTATO, "Hot potato"}}};

const char* name_of(snake::sim::Mode mode)
{
    for (const ModeButton& button : MODES)
    {
        if (button.mode == mode)
        {
            return button.name;
        }
    }
    return "";
}

lv_color_t colour(std::uint32_t rgb)
{
    return lv_color_hex(rgb);
}

/// A seed for a new game from the chip's random number generator.
std::uint64_t new_seed()
{
    return (static_cast<std::uint64_t>(esp_random()) << 32U) | esp_random();
}

/// The food rises and falls a few pixels: a triangle wave of the frame counter, 0 to 5 pixels.
int lift_at(std::uint32_t frame)
{
    constexpr std::uint32_t PERIOD = 14; // frames, at about 15 a second
    const std::uint32_t phase = frame % PERIOD;
    const std::uint32_t up = phase < PERIOD / 2 ? phase : PERIOD - phase;
    return static_cast<int>((up * 5U) / (PERIOD / 2));
}

/// A button with a label, in the style of the design: a cream face with an ink border.
lv_obj_t* make_button(lv_obj_t* parent, const char* text, int width, int height, lv_event_cb_t callback, void* user)
{
    lv_obj_t* button = lv_button_create(parent);
    lv_obj_set_size(button, width, height);
    lv_obj_set_style_bg_color(button, colour(snake::raster::palette::SURFACE), 0);
    lv_obj_set_style_border_color(button, colour(snake::raster::palette::INK), 0);
    lv_obj_set_style_border_width(button, 3, 0);
    lv_obj_set_style_radius(button, 12, 0);
    lv_obj_set_style_shadow_color(button, colour(snake::raster::palette::SHADOW), 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_shadow_offset_y(button, 4, 0);
    lv_obj_set_style_shadow_opa(button, LV_OPA_COVER, 0);
    lv_obj_t* label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, colour(snake::raster::palette::INK), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_28, 0);
    lv_obj_center(label);
    if (callback != nullptr)
    {
        lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, user);
    }
    return button;
}

lv_obj_t* make_label(lv_obj_t* parent, const lv_font_t* font, const char* text)
{
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, colour(snake::raster::palette::INK), 0);
    return label;
}

/// A full-size container with no frame, for one screen of the game.
lv_obj_t* make_page(lv_obj_t* parent)
{
    lv_obj_t* page = lv_obj_create(parent);
    lv_obj_remove_style_all(page);
    lv_obj_set_size(page, lv_pct(100), lv_pct(100));
    return page;
}

} // namespace

SnakeApp::SnakeApp() : m_Session(snake::sim::Mode::EASY, new_seed())
{
    lv_obj_t* screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, colour(0xFFC93C), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(screen, false);

    const int side = snake::raster::board_side(CELL_PIXELS);
    // The board's picture is 620 KB, which belongs in the PSRAM.
    m_Pixels = static_cast<std::uint16_t*>(
        heap_caps_malloc(static_cast<std::size_t>(side) * static_cast<std::size_t>(side) * sizeof(std::uint16_t),
                         MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (m_Pixels == nullptr)
    {
        ESP_LOGE(TAG, "no memory for the picture of the board");
    }

    build_title(screen);
    build_game(screen);
    build_over(screen);
    show_title();

    // A swipe anywhere steers the snake.
    lv_obj_add_event_cb(screen, &SnakeApp::on_gesture, LV_EVENT_GESTURE, this);

    m_LastTickMs = lv_tick_get();
    lv_timer_create(&SnakeApp::on_timer_trampoline, 10, this);
}

void SnakeApp::build_title(lv_obj_t* parent)
{
    m_Title = make_page(parent);
    lv_obj_t* heading = make_label(m_Title, &lv_font_montserrat_48, "Snake");
    lv_obj_align(heading, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_t* prompt = make_label(m_Title, &lv_font_montserrat_28, "Choose a mode");
    lv_obj_align(prompt, LV_ALIGN_TOP_MID, 0, 120);
    int y = 190;
    for (const ModeButton& entry : MODES)
    {
        lv_obj_t* button = make_button(m_Title, entry.name, 420, 70, &SnakeApp::on_mode_clicked, this);
        lv_obj_align(button, LV_ALIGN_TOP_MID, 0, y);
        lv_obj_set_user_data(button, reinterpret_cast<void*>(static_cast<std::uintptr_t>(entry.mode)));
        y += 90;
    }
    // The way back to the firmware that was on the board before this one (see on_exit_clicked).
    lv_obj_t* leave = make_button(m_Title, "Exit", 160, 56, &SnakeApp::on_exit_clicked, this);
    lv_obj_align(leave, LV_ALIGN_BOTTOM_RIGHT, -MARGIN, -MARGIN);
}

void SnakeApp::build_game(lv_obj_t* parent)
{
    m_Game = make_page(parent);
    const int side = snake::raster::board_side(CELL_PIXELS);
    const int board_x = (PANEL_WIDTH - side) / 2;
    const int column = board_x - 2 * MARGIN;
    m_Canvas = lv_canvas_create(m_Game);
    if (m_Pixels != nullptr)
    {
        lv_canvas_set_buffer(m_Canvas, m_Pixels, side, side, LV_COLOR_FORMAT_RGB565);
    }
    lv_obj_align(m_Canvas, LV_ALIGN_TOP_LEFT, board_x, (PANEL_HEIGHT - side) / 2);

    m_ModeLabel = make_label(m_Game, &lv_font_montserrat_28, "");
    lv_obj_align(m_ModeLabel, LV_ALIGN_TOP_LEFT, MARGIN, 40);
    m_ScoreLabel = make_label(m_Game, &lv_font_montserrat_48, "000000");
    lv_obj_align(m_ScoreLabel, LV_ALIGN_TOP_LEFT, MARGIN, 90);

    // Hot potato: the Shed button and the fuse over it.
    m_Fuse = lv_bar_create(m_Game);
    lv_obj_set_size(m_Fuse, column, 16);
    lv_bar_set_range(m_Fuse, 0, 100);
    lv_bar_set_value(m_Fuse, 100, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(m_Fuse, colour(snake::raster::palette::SURFACE), LV_PART_MAIN);
    lv_obj_set_style_bg_color(m_Fuse, colour(snake::raster::palette::ACCENT), LV_PART_INDICATOR);
    lv_obj_align(m_Fuse, LV_ALIGN_BOTTOM_LEFT, MARGIN, -120);
    m_ShedButton = make_button(m_Game, "Shed", column, 80, &SnakeApp::on_shed_clicked, this);
    lv_obj_align(m_ShedButton, LV_ALIGN_BOTTOM_LEFT, MARGIN, -30);

    lv_obj_t* menu = make_button(m_Game, "Menu", column, 56, &SnakeApp::on_menu_clicked, this);
    lv_obj_align(menu, LV_ALIGN_TOP_LEFT, board_x + side + MARGIN, MARGIN);
    build_controls(m_Game);
}

/// A small button that chooses between the ways of steering: a cream face, or a coral one for the choice.
lv_obj_t* SnakeApp::make_chip(lv_obj_t* parent, const char* text, int x, int y, lv_event_cb_t callback)
{
    lv_obj_t* chip = make_button(parent, text, CHIP_WIDTH, CHIP_HEIGHT, callback, this);
    lv_obj_set_style_radius(chip, 10, 0);
    lv_obj_set_style_text_font(lv_obj_get_child(chip, 0), &lv_font_montserrat_20, 0);
    lv_obj_align(chip, LV_ALIGN_TOP_LEFT, x, y);
    return chip;
}

void SnakeApp::build_controls(lv_obj_t* parent)
{
    const int side = snake::raster::board_side(CELL_PIXELS);
    const int column_x = (PANEL_WIDTH + side) / 2 + MARGIN;
    const int column = PANEL_WIDTH - column_x - MARGIN;
    const int plate_x = column_x + (column - STICK_PLATE) / 2;

    // The joystick: the plate takes the touch, and the knob follows it.
    m_StickPlate = lv_obj_create(parent);
    lv_obj_remove_style_all(m_StickPlate);
    lv_obj_set_size(m_StickPlate, STICK_PLATE, STICK_PLATE);
    lv_obj_set_style_radius(m_StickPlate, 28, 0);
    lv_obj_set_style_bg_color(m_StickPlate, colour(snake::raster::palette::SURFACE), 0);
    lv_obj_set_style_bg_opa(m_StickPlate, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(m_StickPlate, colour(snake::raster::palette::INK), 0);
    lv_obj_set_style_border_width(m_StickPlate, 4, 0);
    lv_obj_set_style_shadow_color(m_StickPlate, colour(snake::raster::palette::SHADOW), 0);
    lv_obj_set_style_shadow_width(m_StickPlate, 0, 0);
    lv_obj_set_style_shadow_offset_y(m_StickPlate, 8, 0);
    lv_obj_set_style_shadow_opa(m_StickPlate, LV_OPA_COVER, 0);
    lv_obj_remove_flag(m_StickPlate, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(m_StickPlate, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(m_StickPlate, LV_ALIGN_TOP_LEFT, plate_x, STICK_TOP);
    lv_obj_add_event_cb(m_StickPlate, &SnakeApp::on_stick, LV_EVENT_ALL, this);

    m_StickKnob = lv_obj_create(m_StickPlate);
    lv_obj_remove_style_all(m_StickKnob);
    lv_obj_set_size(m_StickKnob, STICK_KNOB, STICK_KNOB);
    lv_obj_set_style_radius(m_StickKnob, 20, 0);
    lv_obj_set_style_bg_color(m_StickKnob, colour(snake::raster::palette::ACCENT), 0);
    lv_obj_set_style_bg_opa(m_StickKnob, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(m_StickKnob, colour(snake::raster::palette::INK), 0);
    lv_obj_set_style_border_width(m_StickKnob, 4, 0);
    lv_obj_remove_flag(m_StickKnob, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(m_StickKnob, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(m_StickKnob, LV_ALIGN_CENTER, 0, 0);

    // The d-pad: four keys in a cross, in the same place as the joystick.
    m_Pad = make_page(parent);
    lv_obj_set_size(m_Pad, STICK_PLATE, STICK_PLATE);
    lv_obj_align(m_Pad, LV_ALIGN_TOP_LEFT, plate_x, STICK_TOP);
    const int middle = (STICK_PLATE - PAD_KEY) / 2;
    struct PadKey
    {
        const char* symbol;
        int x;
        int y;
        snake::sim::Direction direction;
    };
    const std::array<PadKey, 4> keys = {{{LV_SYMBOL_UP, middle, 0, snake::sim::Direction::UP},
                                         {LV_SYMBOL_LEFT, 0, middle, snake::sim::Direction::LEFT},
                                         {LV_SYMBOL_RIGHT, STICK_PLATE - PAD_KEY, middle, snake::sim::Direction::RIGHT},
                                         {LV_SYMBOL_DOWN, middle, STICK_PLATE - PAD_KEY, snake::sim::Direction::DOWN}}};
    for (const PadKey& key : keys)
    {
        lv_obj_t* button = make_button(m_Pad, key.symbol, PAD_KEY, PAD_KEY, nullptr, this);
        lv_obj_add_event_cb(button, &SnakeApp::on_pad, LV_EVENT_PRESSED, this);
        lv_obj_align(button, LV_ALIGN_TOP_LEFT, key.x, key.y);
        lv_obj_set_user_data(button, reinterpret_cast<void*>(static_cast<std::uintptr_t>(key.direction)));
    }

    // Swiping needs no control of its own: the note says where.
    m_SwipeNote = make_label(parent, &lv_font_montserrat_28, "Swipe on\nthe screen");
    lv_obj_set_style_text_align(m_SwipeNote, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(m_SwipeNote, LV_ALIGN_TOP_LEFT, plate_x + 12, STICK_TOP + 56);

    // The choice: three ways to steer, and tilt, which is listed and off because the board has no motion sensor.
    const int chips_y = STICK_TOP + STICK_PLATE + 36;
    const int second_x = column_x + column - CHIP_WIDTH;
    m_StickChip = make_chip(parent, "Stick", column_x, chips_y, &SnakeApp::on_controls_chip);
    m_PadChip = make_chip(parent, "D-pad", second_x, chips_y, &SnakeApp::on_controls_chip);
    m_SwipeChip = make_chip(parent, "Swipe", column_x, chips_y + CHIP_HEIGHT + 8, &SnakeApp::on_controls_chip);
    lv_obj_t* tilt = make_chip(parent, "Tilt", second_x, chips_y + CHIP_HEIGHT + 8, nullptr);
    lv_obj_add_state(tilt, LV_STATE_DISABLED);
    lv_obj_set_style_opa(tilt, LV_OPA_50, 0);
    lv_obj_t* note = make_label(parent, &lv_font_montserrat_20, "Tilt needs a sensor");
    lv_obj_align(note, LV_ALIGN_TOP_LEFT, column_x, chips_y + 2 * CHIP_HEIGHT + 24);

    set_controls(Controls::JOYSTICK);
}

void SnakeApp::build_over(lv_obj_t* parent)
{
    m_Over = make_page(parent);
    lv_obj_t* heading = make_label(m_Over, &lv_font_montserrat_48, "Game over");
    lv_obj_align(heading, LV_ALIGN_TOP_MID, 0, 120);
    m_OverLabel = make_label(m_Over, &lv_font_montserrat_28, "");
    lv_obj_align(m_OverLabel, LV_ALIGN_TOP_MID, 0, 200);
    lv_obj_t* again = make_button(m_Over, "Play again", 360, 80, &SnakeApp::on_again_clicked, this);
    lv_obj_align(again, LV_ALIGN_TOP_MID, 0, 290);
    lv_obj_t* menu = make_button(m_Over, "Menu", 360, 80, &SnakeApp::on_menu_clicked, this);
    lv_obj_align(menu, LV_ALIGN_TOP_MID, 0, 390);
}

void SnakeApp::show_title()
{
    lv_obj_set_hidden(m_Title, false);
    lv_obj_set_hidden(m_Game, true);
    lv_obj_set_hidden(m_Over, true);
}

void SnakeApp::show_over()
{
    lv_obj_set_hidden(m_Title, true);
    lv_obj_set_hidden(m_Game, true);
    lv_obj_set_hidden(m_Over, false);
    lv_label_set_text_fmt(m_OverLabel, "%s: %u points", name_of(m_Mode),
                          static_cast<unsigned>(m_Session.simulation().score()));
}

void SnakeApp::start_game(snake::sim::Mode mode)
{
    m_Mode = mode;
    m_Session = snake::sim::Session(mode, new_seed());
    static_cast<void>(m_Session.start());
    let_go_of_stick();
    m_Pacer.reset();
    m_LastTickMs = lv_tick_get();
    lv_obj_set_hidden(m_Title, true);
    lv_obj_set_hidden(m_Over, true);
    lv_obj_set_hidden(m_Game, false);
    lv_label_set_text(m_ModeLabel, name_of(mode));
    if (mode == snake::sim::Mode::HOT_POTATO)
    {
        lv_obj_set_hidden(m_ShedButton, false);
        lv_obj_set_hidden(m_Fuse, false);
    }
    else
    {
        lv_obj_set_hidden(m_ShedButton, true);
        lv_obj_set_hidden(m_Fuse, true);
    }
    refresh();
}

void SnakeApp::on_timer()
{
    const std::uint32_t now = lv_tick_get();
    const std::uint32_t elapsed_ms = now - m_LastTickMs; // unsigned: right across the wrap of the tick counter
    m_LastTickMs = now;
    ++m_Frame;
    if (m_Session.state() != snake::sim::GameState::RUNNING)
    {
        return;
    }
    if (m_Controls == Controls::JOYSTICK)
    {
        // A thumb held still still steers: the heading it is read against changes with each turn.
        if (const auto direction = m_Stick.again(m_Session.simulation()))
        {
            turn(*direction);
        }
    }
    m_Pacer.add(elapsed_ms * 1000U);
    bool ticked = false;
    while (m_Session.state() == snake::sim::GameState::RUNNING && m_Pacer.due(m_Session.tick_interval_ms()))
    {
        static_cast<void>(m_Session.tick());
        ticked = true;
    }
    // The picture changes with a tick, and the food rises and falls about fifteen times a second.
    if (ticked || m_Frame % 6 == 0)
    {
        refresh();
    }
    if (m_Session.state() == snake::sim::GameState::GAME_OVER)
    {
        show_over();
    }
}

void SnakeApp::refresh()
{
    if (m_Pixels == nullptr)
    {
        return;
    }
    const int side = snake::raster::board_side(CELL_PIXELS);
    const snake::sim::Simulation& game = m_Session.simulation();
    snake::raster::paint_board({.pixels = m_Pixels, .width = side, .height = side}, CELL_PIXELS, game,
                               {.lift = lift_at(m_Frame / 6), .spark = (m_Frame / 6) % 2 == 0});
    lv_obj_invalidate(m_Canvas);
    lv_label_set_text_fmt(m_ScoreLabel, "%06u", static_cast<unsigned>(game.score()));

    if (m_Mode == snake::sim::Mode::HOT_POTATO)
    {
        const auto index = game.lump_index();
        const bool can_shed = m_Session.state() == snake::sim::GameState::RUNNING && index && *index >= 1;
        if (can_shed)
        {
            lv_obj_remove_state(m_ShedButton, LV_STATE_DISABLED);
        }
        else
        {
            lv_obj_add_state(m_ShedButton, LV_STATE_DISABLED);
        }
        // The fuse burns down as the bomb goes down the body: 100 when it has just been swallowed, 0 on the tail.
        int fuse = 100;
        if (index && game.length() >= 2)
        {
            fuse = static_cast<int>((100U * (game.length() - 1U - *index)) / (game.length() - 1U));
        }
        lv_bar_set_value(m_Fuse, fuse, LV_ANIM_OFF);
    }
}

void SnakeApp::steer(lv_dir_t direction)
{
    switch (direction)
    {
    case LV_DIR_TOP:
        turn(snake::sim::Direction::UP);
        break;
    case LV_DIR_BOTTOM:
        turn(snake::sim::Direction::DOWN);
        break;
    case LV_DIR_LEFT:
        turn(snake::sim::Direction::LEFT);
        break;
    case LV_DIR_RIGHT:
        turn(snake::sim::Direction::RIGHT);
        break;
    default:
        break;
    }
}

void SnakeApp::turn(snake::sim::Direction direction)
{
    // The game drops a turn back into the snake and a repeat of the direction already in force.
    static_cast<void>(m_Session.turn(direction));
}

void SnakeApp::set_controls(Controls controls)
{
    m_Controls = controls;
    let_go_of_stick();
    lv_obj_set_hidden(m_StickPlate, controls != Controls::JOYSTICK);
    lv_obj_set_hidden(m_Pad, controls != Controls::DPAD);
    lv_obj_set_hidden(m_SwipeNote, controls != Controls::SWIPE);
    const auto choose = [controls](lv_obj_t* chip, Controls which)
    {
        lv_obj_set_style_bg_color(
            chip, colour(controls == which ? snake::raster::palette::ACCENT : snake::raster::palette::SURFACE), 0);
    };
    choose(m_StickChip, Controls::JOYSTICK);
    choose(m_PadChip, Controls::DPAD);
    choose(m_SwipeChip, Controls::SWIPE);
}

void SnakeApp::follow_stick(int dx, int dy)
{
    const auto travel = static_cast<float>(STICK_TRAVEL);
    const float length = std::hypot(static_cast<float>(dx), static_cast<float>(dy));
    float knob_x = static_cast<float>(dx);
    float knob_y = static_cast<float>(dy);
    if (length > travel)
    {
        knob_x *= travel / length;
        knob_y *= travel / length;
    }
    lv_obj_align(m_StickKnob, LV_ALIGN_CENTER, static_cast<int>(knob_x), static_cast<int>(knob_y));
    // Right and up are positive for the joystick, and the screen's y runs down.
    const float right = knob_x / travel;
    const float up = -knob_y / travel;
    if (m_Session.state() == snake::sim::GameState::RUNNING)
    {
        if (const auto direction = m_Stick.move(right, up, m_Session.simulation()))
        {
            turn(*direction);
        }
    }
    else
    {
        m_Stick.hold(right, up);
    }
}

void SnakeApp::let_go_of_stick()
{
    m_Stick.release();
    if (m_StickKnob != nullptr)
    {
        lv_obj_align(m_StickKnob, LV_ALIGN_CENTER, 0, 0);
    }
}

void SnakeApp::on_timer_trampoline(lv_timer_t* timer)
{
    static_cast<SnakeApp*>(lv_timer_get_user_data(timer))->on_timer();
}

void SnakeApp::on_mode_clicked(lv_event_t* event)
{
    auto* app = static_cast<SnakeApp*>(lv_event_get_user_data(event));
    const auto mode = static_cast<snake::sim::Mode>(
        reinterpret_cast<std::uintptr_t>(lv_obj_get_user_data(static_cast<lv_obj_t*>(lv_event_get_target(event)))));
    app->start_game(mode);
}

void SnakeApp::on_gesture(lv_event_t* event)
{
    auto* app = static_cast<SnakeApp*>(lv_event_get_user_data(event));
    if (app->m_Controls == Controls::SWIPE)
    {
        app->steer(lv_indev_get_gesture_dir(lv_indev_active()));
    }
}

void SnakeApp::on_stick(lv_event_t* event)
{
    auto* app = static_cast<SnakeApp*>(lv_event_get_user_data(event));
    switch (lv_event_get_code(event))
    {
    case LV_EVENT_PRESSED:
    case LV_EVENT_PRESSING:
    {
        lv_point_t point;
        lv_indev_get_point(lv_indev_active(), &point);
        lv_area_t area;
        lv_obj_get_coords(app->m_StickPlate, &area);
        app->follow_stick(point.x - (area.x1 + area.x2) / 2, point.y - (area.y1 + area.y2) / 2);
        break;
    }
    case LV_EVENT_RELEASED:
    case LV_EVENT_PRESS_LOST:
        app->let_go_of_stick();
        break;
    default:
        break;
    }
}

void SnakeApp::on_pad(lv_event_t* event)
{
    auto* app = static_cast<SnakeApp*>(lv_event_get_user_data(event));
    const auto direction = static_cast<snake::sim::Direction>(
        reinterpret_cast<std::uintptr_t>(lv_obj_get_user_data(static_cast<lv_obj_t*>(lv_event_get_target(event)))));
    app->turn(direction);
}

void SnakeApp::on_controls_chip(lv_event_t* event)
{
    auto* app = static_cast<SnakeApp*>(lv_event_get_user_data(event));
    const lv_obj_t* chip = static_cast<lv_obj_t*>(lv_event_get_target(event));
    if (chip == app->m_StickChip)
    {
        app->set_controls(Controls::JOYSTICK);
    }
    else if (chip == app->m_PadChip)
    {
        app->set_controls(Controls::DPAD);
    }
    else if (chip == app->m_SwipeChip)
    {
        app->set_controls(Controls::SWIPE);
    }
}

void SnakeApp::on_shed_clicked(lv_event_t* event)
{
    auto* app = static_cast<SnakeApp*>(lv_event_get_user_data(event));
    if (app->m_Session.shed())
    {
        app->refresh();
    }
}

void SnakeApp::on_exit_clicked(lv_event_t* /*event*/)
{
    // Snake never marks itself as good (esp_ota_mark_app_valid_cancel_rollback), so a board that was updated over the
    // network with it goes back to the firmware it had at its next restart. This does that now: the bootloader boots
    // the slot it came from. On a board that was flashed over USB there is no other slot, and it fails.
    const esp_err_t error = esp_ota_mark_app_invalid_rollback_and_reboot();
    ESP_LOGW(TAG, "there is no firmware to go back to: %s", esp_err_to_name(error));
}

void SnakeApp::on_menu_clicked(lv_event_t* event)
{
    static_cast<SnakeApp*>(lv_event_get_user_data(event))->show_title();
}

void SnakeApp::on_again_clicked(lv_event_t* event)
{
    auto* app = static_cast<SnakeApp*>(lv_event_get_user_data(event));
    app->start_game(app->m_Mode);
}

} // namespace snake::esp
