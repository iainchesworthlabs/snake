// SPDX-License-Identifier: GPL-3.0-or-later
#include "about/about_info.hpp"

#include "snake/source_info.hpp"
#include "snake/version.hpp"

#include <QFile>
#include <QIODevice>
#include <QString>

namespace snake::ui
{

namespace
{

QString read_resource(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return QStringLiteral("(missing: %1)").arg(path);
    }
    return QString::fromUtf8(file.readAll());
}

} // namespace

AboutInfo::AboutInfo(QObject* parent) : QObject(parent) {}

QString AboutInfo::version()
{
    return QString::fromUtf8(VERSION_STRING.data(), static_cast<qsizetype>(VERSION_STRING.size()));
}

QString AboutInfo::licence()
{
    return QStringLiteral("GPL-3.0-or-later");
}

QString AboutInfo::repositoryUrl()
{
    return QString::fromUtf8(REPOSITORY_URL.data(), static_cast<qsizetype>(REPOSITORY_URL.size()));
}

QString AboutInfo::sourceRevision()
{
    return QString::fromUtf8(SOURCE_REVISION.data(), static_cast<qsizetype>(SOURCE_REVISION.size()));
}

QString AboutInfo::licenceText()
{
    return read_resource(QStringLiteral(":/snake/about/licence.txt"));
}

QString AboutInfo::noticesText()
{
    return read_resource(QStringLiteral(":/snake/about/notices.txt"));
}

QString AboutInfo::fontLicenceText()
{
    return read_resource(QStringLiteral(":/snake/about/font-licence.txt"));
}

} // namespace snake::ui
