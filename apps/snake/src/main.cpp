// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform/touch_screen.hpp"
#include "snake/version.hpp"
#include "snake/window_settings.hpp"
#include "theme/theme.hpp"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QString>
#include <QSurfaceFormat>
#include <QTranslator>
#include <QVariantMap>

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("snake"));
    QGuiApplication::setApplicationVersion(
        QString::fromUtf8(snake::VERSION_STRING.data(), snake::VERSION_STRING.size()));

    // A font that fails to load leaves the fallback families of the font in use, so the game still runs.
    snake::ui::load_fonts();
    QGuiApplication::setFont(snake::ui::application_font());

    // The catalogues are compiled into the executable. A language without one shows the English source.
    QTranslator translator;
    if (translator.load(QLocale(), QStringLiteral("snake"), QStringLiteral("_"), QStringLiteral(":/i18n")))
    {
        QGuiApplication::installTranslator(&translator);
    }

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Snake"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption smoke_test(QStringLiteral("smoke-test"),
                                        QStringLiteral("Exit after the first frame has been shown."));
    parser.addOption(smoke_test);
    const QCommandLineOption save_file(
        QStringLiteral("save-file"),
        QStringLiteral("Keep the leaderboards and settings in <file> instead of the user's data folder."),
        QStringLiteral("file"));
    parser.addOption(save_file);
    parser.process(app);

    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    format.setSamples(snake::MAIN_WINDOW.multisample_count);
    QSurfaceFormat::setDefaultFormat(format);

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
        Qt::QueuedConnection);
    // The leaderboards, recent names, unlocks and settings live in one file in the user's data folder.
    const QString save_path =
        parser.isSet(save_file)
            ? parser.value(save_file)
            : QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/save.snk");
    engine.setInitialProperties({{QStringLiteral("windowWidth"), snake::MAIN_WINDOW.width},
                                 {QStringLiteral("windowHeight"), snake::MAIN_WINDOW.height},
                                 {QStringLiteral("saveFile"), save_path},
                                 {QStringLiteral("touchDevice"), snake::ui::has_touch_screen()}});
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
