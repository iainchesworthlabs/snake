// SPDX-License-Identifier: GPL-3.0-or-later
#include "theme/theme.hpp"
#include "theme/tokens.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QByteArray>
#include <QColor>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <cstdint>
#include <vector>

namespace tokens = snake::ui::tokens;
using snake::ui::Theme;

namespace
{

struct ColourToken
{
    const char* name;
    std::uint32_t value;
};

const std::vector<ColourToken>& colour_tokens()
{
    static const std::vector<ColourToken> all = {
        {"background", tokens::BACKGROUND},
        {"ink", tokens::INK},
        {"shadow", tokens::SHADOW},
        {"surface", tokens::SURFACE},
        {"accent", tokens::ACCENT},
        {"boardLight", tokens::BOARD_LIGHT},
        {"boardDark", tokens::BOARD_DARK},
        {"food", tokens::FOOD},
        {"foodHighlight", tokens::FOOD_HIGHLIGHT},
        {"leaf", tokens::LEAF},
        {"tongue", tokens::TONGUE},
        {"eye", tokens::EYE},
        {"medalGold", tokens::MEDAL_GOLD},
        {"medalSilver", tokens::MEDAL_SILVER},
        {"medalBronze", tokens::MEDAL_BRONZE},
    };
    return all;
}

QJsonObject read_tokens()
{
    QFile file(QStringLiteral(SNAKE_TOKENS_JSON));
    REQUIRE(file.open(QIODevice::ReadOnly));
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    REQUIRE(document.isObject());
    return document.object();
}

} // namespace

TEST_CASE("every colour token matches docs/design/tokens.json", "[theme]")
{
    const QJsonObject colours = read_tokens().value(QStringLiteral("color")).toObject();
    for (const auto& token : colour_tokens())
    {
        INFO(token.name);
        const QString text = colours.value(QString::fromLatin1(token.name)).toObject().value("value").toString();
        REQUIRE(QColor(text).isValid());
        CHECK(QColor(text).rgb() == QColor::fromRgb(token.value).rgb());
    }
}

TEST_CASE("every opaque colour in tokens.json has a constant", "[theme]")
{
    const QJsonObject colours = read_tokens().value(QStringLiteral("color")).toObject();
    QStringList known;
    for (const auto& token : colour_tokens())
    {
        known.push_back(QString::fromLatin1(token.name));
    }
    for (auto it = colours.begin(); it != colours.end(); ++it)
    {
        const QString text = it.value().toObject().value("value").toString();
        // The overlay is the only colour with an alpha channel and is drawn with its own rule.
        if (text.startsWith(QLatin1Char('#')))
        {
            INFO(it.key().toStdString());
            CHECK(known.contains(it.key()));
        }
    }
}

TEST_CASE("the font family constant is the family of tokens.json", "[theme]")
{
    const QJsonObject typography = read_tokens().value(QStringLiteral("typography")).toObject();
    CHECK(typography.value(QStringLiteral("family")).toString().toStdString() == std::string(tokens::FONT_FAMILY));
}

TEST_CASE("the bundled fonts load and the application font resolves to them", "[theme]")
{
    REQUIRE(snake::ui::load_fonts());
    const QString family = QString::fromLatin1(tokens::FONT_FAMILY.data(), tokens::FONT_FAMILY.size());
    REQUIRE(QFontDatabase::families().contains(family));

    // All four weights are present.
    const QStringList styles = QFontDatabase::styles(family);
    CHECK(styles.size() >= 4);

    const QFont font = snake::ui::application_font();
    CHECK(font.families().first() == family);
    CHECK(QFontInfo(font).family() == family);
}

TEST_CASE("the theme gives the colours and the family to QML", "[theme]")
{
    const Theme theme;
    CHECK(theme.background() == QColor::fromRgb(tokens::BACKGROUND));
    CHECK(theme.ink() == QColor::fromRgb(tokens::INK));
    CHECK(theme.shadow() == QColor::fromRgb(tokens::SHADOW));
    CHECK(theme.surface() == QColor::fromRgb(tokens::SURFACE));
    CHECK(theme.accent() == QColor::fromRgb(tokens::ACCENT));
    CHECK(theme.fontFamily() == QStringLiteral("Pixelify Sans"));
}
