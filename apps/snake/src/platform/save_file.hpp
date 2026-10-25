// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/profile/save_data.hpp"

#include <QString>
#include <cstdint>
#include <expected>

namespace snake::ui
{

/// The platform service that keeps the save data in one file. The profile library turns the data into bytes and back;
/// this class reads and writes the bytes. A write replaces the whole file or leaves the old one as it was, so a crash
/// or a full disk never leaves half a save.
class SaveFile
{
public:
    enum class Error : std::uint8_t
    {
        MISSING,      // there is no file yet: the first run
        UNREADABLE,   // the file is there and could not be read, or could not be written
        DAMAGED,      // the file is not a save, or fails its checksum; load() moved it aside
        NEWER_VERSION // written by a newer program: it must not be overwritten
    };

    explicit SaveFile(QString path);

    [[nodiscard]] const QString& path() const noexcept
    {
        return m_Path;
    }

    /// The path a damaged file is moved to, next to the save file.
    [[nodiscard]] QString damaged_path() const;

    /// Reads and checks the file. A damaged file is renamed to damaged_path(), replacing an older one, so the next
    /// save does not destroy it and the player or a developer can look at it.
    [[nodiscard]] std::expected<profile::SaveData, Error> load() const;

    /// Writes `data`, creating the folder when it is missing. Returns UNREADABLE when it cannot (the name stands for
    /// any failure to reach the file).
    [[nodiscard]] std::expected<void, Error> save(const profile::SaveData& data) const;

private:
    QString m_Path;
};

} // namespace snake::ui
