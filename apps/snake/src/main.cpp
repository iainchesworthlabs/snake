// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/version.hpp"
#include "snake/window_settings.hpp"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QString>
#include <QSurfaceFormat>
#include <QVariantMap>

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("snake"));
    QGuiApplication::setApplicationVersion(
        QString::fromUtf8(snake::VERSION_STRING.data(), snake::VERSION_STRING.size()));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Snake"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption smoke_test(QStringLiteral("smoke-test"),
                                        QStringLiteral("Exit after the first frame has been shown."));
    parser.addOption(smoke_test);
    parser.process(app);

    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    format.setSamples(snake::MAIN_WINDOW.multisample_count);
    QSurfaceFormat::setDefaultFormat(format);

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
        Qt::QueuedConnection);
    engine.setInitialProperties({{QStringLiteral("windowWidth"), snake::MAIN_WINDOW.width},
                                 {QStringLiteral("windowHeight"), snake::MAIN_WINDOW.height}});
    engine.loadFromModule("SnakeApp", "Main");

    if (parser.isSet(smoke_test))
    {
        const auto roots = engine.rootObjects();
        auto* window = roots.isEmpty() ? nullptr : qobject_cast<QQuickWindow*>(roots.first());
        if (window == nullptr)
        {
            return 1;
        }
        QObject::connect(window, &QQuickWindow::frameSwapped, &app, [] { QCoreApplication::quit(); });
    }

    return QGuiApplication::exec();
}
