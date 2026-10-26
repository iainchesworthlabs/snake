// SPDX-License-Identifier: GPL-3.0-or-later
#include "language/language_choice.hpp"

#include <QString>
#include <QStringList>
#include <QStringView>

namespace snake::ui
{

namespace
{

QString g_description;

/// The code of the catalogue that serves a tag, or an empty string when the game has none for it.
QString catalogue_for(const QString& tag, const QStringList& available)
{
    const QStringList parts = QString(tag).replace(u'_', u'-').split(u'-', Qt::SkipEmptyParts);
    if (parts.isEmpty())
    {
        return {};
    }
    QString language = parts.front().toLower();
    // The old codes that systems still send for Hebrew and Yiddish.
    if (language == QStringLiteral("iw"))
    {
        language = QStringLiteral("he");
    }
    else if (language == QStringLiteral("ji"))
    {
        language = QStringLiteral("yi");
    }

    QString code = language;
    if (language == QStringLiteral("zh"))
    {
        // Simplified, the one the game has, unless the tag says Traditional.
        for (const QString& part : parts)
        {
            const QString lower = part.toLower();
            if (lower == QStringLiteral("hant") || lower == QStringLiteral("tw") || lower == QStringLiteral("hk") ||
                lower == QStringLiteral("mo"))
            {
                return {};
            }
        }
        code = QStringLiteral("zh_CN");
    }
    return available.contains(code) ? code : QString();
}

} // namespace

QStringList available_languages()
{
    return {QStringLiteral("en"), QStringLiteral("fr"),    QStringLiteral("es"), QStringLiteral("de"),
            QStringLiteral("ja"), QStringLiteral("zh_CN"), QStringLiteral("he"), QStringLiteral("yi")};
}

LanguageChoice choose_language(const QStringList& preferred, const QStringList& available)
{
    for (const QString& tag : preferred)
    {
        const QString code = catalogue_for(tag, available);
        if (!code.isEmpty())
        {
            return {code, tag};
        }
    }
    return {QStringLiteral("en"), {}};
}

void record_language_choice(const LanguageChoice& choice, const QStringList& preferred)
{
    g_description = QStringLiteral("%1 (%2; the system offers %3)")
                        .arg(choice.code,
                             choice.matched.isEmpty() ? QStringLiteral("nothing matched")
                                                      : QStringLiteral("from ") + choice.matched,
                             preferred.isEmpty() ? QStringLiteral("nothing") : preferred.join(QStringLiteral(", ")));
}

QString language_choice_description()
{
    return g_description;
}

} // namespace snake::ui
