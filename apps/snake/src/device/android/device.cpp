// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/device.hpp"

#include "device/haptics.hpp"
#include "device/qt_accelerometer.hpp"
#include "snake/input/sample_source.hpp"

#include <QCoreApplication>
#include <QJniEnvironment>
#include <QJniObject>
#include <QString>
#include <memory>

namespace snake::ui::device
{

namespace
{

/// The vibrator of the phone, through the system's Vibrator service and a one-shot VibrationEffect (API 26; the game
/// needs 28). It needs the VIBRATE permission, which Android grants at install and never asks the person about.
class AndroidHaptics final : public Haptics
{
public:
    void tap() override
    {
        vibrate(15, 90);
    }

    void thud() override
    {
        vibrate(70, 255);
    }

private:
    static void vibrate(int milliseconds, int amplitude)
    {
        const QJniObject context = QNativeInterface::QAndroidApplication::context();
        if (!context.isValid())
        {
            return;
        }
        const QJniObject name = QJniObject::fromString(QStringLiteral("vibrator"));
        const QJniObject vibrator = context.callObjectMethod(
            "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;", name.object<jstring>());
        if (vibrator.isValid())
        {
            const QJniObject effect = QJniObject::callStaticObjectMethod(
                "android/os/VibrationEffect", "createOneShot", "(JI)Landroid/os/VibrationEffect;",
                static_cast<jlong>(milliseconds), static_cast<jint>(amplitude));
            if (effect.isValid())
            {
                vibrator.callMethod<void>("vibrate", "(Landroid/os/VibrationEffect;)V", effect.object());
            }
        }
        // A phone without a motor, or a permission lost, throws in Java: it is not a reason to end the game.
        QJniEnvironment().checkAndClearExceptions();
    }
};

} // namespace

std::unique_ptr<input::SampleSource> make_accelerometer()
{
    return make_qt_accelerometer();
}

std::unique_ptr<Haptics> make_haptics()
{
    return std::make_unique<AndroidHaptics>();
}

} // namespace snake::ui::device
