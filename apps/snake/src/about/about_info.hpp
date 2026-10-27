// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtQml/qqmlregistration.h>

#include <QObject>
#include <QString>

namespace snake::ui
{

/// What the About screen shows: the version, the licence, where the source is and which revision this is, and the
/// text of the licences. The texts are compiled into the executable, so they are always the ones of this build: the
/// licence of the program, the notices of the third-party components (generated from the vcpkg manifest when CMake
/// configures) and the licence of the bundled font.
class AboutInfo : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString version READ version CONSTANT FINAL)
    Q_PROPERTY(QString licence READ licence CONSTANT FINAL)
    Q_PROPERTY(QString repositoryUrl READ repositoryUrl CONSTANT FINAL)
    Q_PROPERTY(QString sourceRevision READ sourceRevision CONSTANT FINAL)
    /// The revision as About shows it: a commit is its first seven characters, as git shows it, and a tag stays as it
    /// is.
    Q_PROPERTY(QString shortRevision READ shortRevision CONSTANT FINAL)
    Q_PROPERTY(QString languageDetail READ languageDetail CONSTANT FINAL)
    Q_PROPERTY(QString licenceText READ licenceText CONSTANT FINAL)
    Q_PROPERTY(QString noticesText READ noticesText CONSTANT FINAL)
    Q_PROPERTY(QString fontLicenceText READ fontLicenceText CONSTANT FINAL)

public:
    explicit AboutInfo(QObject* parent = nullptr);

    [[nodiscard]] static QString version();
    /// The SPDX name of the licence of the program.
    [[nodiscard]] static QString licence();
    [[nodiscard]] static QString repositoryUrl();
    [[nodiscard]] static QString sourceRevision();
    [[nodiscard]] static QString shortRevision();
    /// `revision` as About shows it: 12 or more hexadecimal digits (a commit) become the first 7, anything else is as
    /// it was.
    [[nodiscard]] static QString shortened(const QString& revision);
    /// Which language the game chose, and the languages the system offered, in its order. For finding out why a device
    /// got a language it should not have. Not translated: it is for the person who reports it.
    [[nodiscard]] static QString languageDetail();
    [[nodiscard]] static QString licenceText();
    [[nodiscard]] static QString noticesText();
    [[nodiscard]] static QString fontLicenceText();
};

} // namespace snake::ui
