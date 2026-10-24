// SPDX-License-Identifier: GPL-3.0-or-later
#include <catch2/catch_session.hpp>

#include <QGuiApplication>
#include <QtGlobal>

// The board item is drawn by a window, which needs a GUI application for the whole run. The tests draw without a
// display, with the software renderer, whatever the environment says; CTest also lists the tests by running this
// program, where nothing sets those variables.
int main(int argc, char* argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QUICK_BACKEND", "software");
    qputenv("QSG_RENDER_LOOP", "basic");
    const QGuiApplication app(argc, argv);
    return Catch::Session().run(argc, argv);
}
