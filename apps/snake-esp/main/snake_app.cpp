// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake_app.hpp"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "lvgl.h"
#include "snake/raster/board_painter.hpp"
#include "snake/raster/palette.hpp"
#include "snake/sim/direction.hpp"
#include "snake/sim/mode.hpp"
#include "snake/sim/session.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace snake::esp
{

namespace
{

constexpr const char* TAG = "snake";

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
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, user);
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
}

void SnakeApp::build_game(lv_obj_t* parent)
{
    m_Game = make_page(parent);
    const int side = snake::raster::board_side(CELL_PIXELS);
    m_Canvas = lv_canvas_create(m_Game);
    if (m_Pixels != nullptr)
    {
        lv_canvas_set_buffer(m_Canvas, m_Pixels, side, side, LV_COLOR_FORMAT_RGB565);
    }
    lv_obj_align(m_Canvas, LV_ALIGN_LEFT_MID, 24, 0);

    m_ModeLabel = make_label(m_Game, &lv_font_montserrat_28, "");
    lv_obj_align(m_ModeLabel, LV_ALIGN_TOP_LEFT, 24 + side + 40, 40);
    m_ScoreLabel = make_label(m_Game, &lv_font_montserrat_48, "000000");
    lv_obj_align(m_ScoreLabel, LV_ALIGN_TOP_LEFT, 24 + side + 40, 100);

    // Hot potato: the Shed button and the fuse over it.
    m_Fuse = lv_bar_create(m_Game);
    lv_obj_set_size(m_Fuse, 220, 16);
    lv_bar_set_range(m_Fuse, 0, 100);
    lv_bar_set_value(m_Fuse, 100, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(m_Fuse, colour(snake::raster::palette::SURFACE), LV_PART_MAIN);
    lv_obj_set_style_bg_color(m_Fuse, colour(snake::raster::palette::ACCENT), LV_PART_INDICATOR);
    lv_obj_align(m_Fuse, LV_ALIGN_BOTTOM_LEFT, 24 + side + 40, -120);
    m_ShedButton = make_button(m_Game, "Shed", 220, 80, &SnakeApp::on_shed_clicked, this);
    lv_obj_align(m_ShedButton, LV_ALIGN_BOTTOM_LEFT, 24 + side + 40, -30);

    lv_obj_t* menu = make_button(m_Game, "Menu", 180, 60, &SnakeApp::on_menu_clicked, this);
    lv_obj_align(menu, LV_ALIGN_TOP_RIGHT, -24, 24);
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
        static_cast<void>(m_Session.turn(snake::sim::Direction::UP));
        break;
    case LV_DIR_BOTTOM:
        static_cast<void>(m_Session.turn(snake::sim::Direction::DOWN));
        break;
    case LV_DIR_LEFT:
        static_cast<void>(m_Session.turn(snake::sim::Direction::LEFT));
        break;
    case LV_DIR_RIGHT:
        static_cast<void>(m_Session.turn(snake::sim::Direction::RIGHT));
        break;
    default:
        break;
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
    app->steer(lv_indev_get_gesture_dir(lv_indev_active()));
}

void SnakeApp::on_shed_clicked(lv_event_t* event)
{
    auto* app = static_cast<SnakeApp*>(lv_event_get_user_data(event));
    if (app->m_Session.shed())
    {
        app->refresh();
    }
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
