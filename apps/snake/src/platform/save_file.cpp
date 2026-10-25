// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform/save_file.hpp"

#include "snake/profile/save_data.hpp"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QSaveFile>
#include <QString>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <utility>

namespace snake::ui
{

SaveFile::SaveFile(QString path) : m_Path(std::move(path)) {}

QString SaveFile::damaged_path() const
{
    return m_Path + QStringLiteral(".damaged");
}

std::expected<profile::SaveData, SaveFile::Error> SaveFile::load() const
{
    QFile file(m_Path);
    if (!file.exists())
    {
        return std::unexpected(Error::MISSING);
    }
    if (!file.open(QIODevice::ReadOnly))
    {
        return std::unexpected(Error::UNREADABLE);
    }
    const QByteArray bytes = file.readAll();
    if (file.error() != QFileDevice::NoError)
    {
        return std::unexpected(Error::UNREADABLE);
    }
    file.close();

    const auto decoded = profile::decode_save(
        std::span(reinterpret_cast<const std::uint8_t*>(bytes.constData()), static_cast<std::size_t>(bytes.size())));
    if (decoded)
    {
        return *decoded;
    }
    if (decoded.error() == profile::SaveError::NEWER_VERSION)
    {
        return std::unexpected(Error::NEWER_VERSION);
    }
    QFile::remove(damaged_path());
    if (!file.rename(damaged_path()))
    {
        return std::unexpected(Error::UNREADABLE);
    }
    return std::unexpected(Error::DAMAGED);
}

std::expected<void, SaveFile::Error> SaveFile::save(const profile::SaveData& data) const
{
    std::array<std::uint8_t, profile::MAX_ENCODED_SAVE_SIZE> bytes{};
    // MAX_ENCODED_SAVE_SIZE is always enough, so the size is never 0.
    const std::size_t size = profile::encode_save(data, bytes);
    if (!QDir().mkpath(QFileInfo(m_Path).absolutePath()))
    {
        return std::unexpected(Error::UNREADABLE);
    }

    QSaveFile file(m_Path);
    if (!file.open(QIODevice::WriteOnly) ||
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<qint64>(size)) !=
            static_cast<qint64>(size) ||
        !file.commit())
    {
        return std::unexpected(Error::UNREADABLE);
    }
    return {};
}

} // namespace snake::ui
