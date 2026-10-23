// SPDX-License-Identifier: GPL-3.0-or-later
#include <catch2/catch_session.hpp>

#include <QCoreApplication>

// QTimer needs a QCoreApplication for the whole run, so the tests have their own main.
int main(int argc, char* argv[])
{
    const QCoreApplication app(argc, argv);
    return Catch::Session().run(argc, argv);
}
