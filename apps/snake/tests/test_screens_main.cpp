// SPDX-License-Identifier: GPL-3.0-or-later
#include "theme/theme.hpp"

#include <catch2/catch_session.hpp>

#include <QGuiApplication>
#include <QtGlobal>

// The screens are drawn by a window, which needs a GUI application for the whole run. The tests draw without a
// display, with the software renderer, whatever the environment says; CTest also lists the tests by running this
// program, where nothing sets those variables. The fonts are the application's own, so the text has the size it has
// for the player.
int main(int argc, char* argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QUICK_BACKEND", "software");
    qputenv("QSG_RENDER_LOOP", "basic");
    const QGuiApplication app(argc, argv);
    snake::ui::load_fonts();
    QGuiApplication::setFont(snake::ui::application_font());
    return Catch::Session().run(argc, argv);
}
