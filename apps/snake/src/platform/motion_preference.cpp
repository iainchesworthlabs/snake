// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform/motion_preference.hpp"

#include <QAccessibilityHints>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QStyleHints>
#include <Qt>

namespace snake::ui
{

bool system_prefers_reduced_motion()
{
    if (qobject_cast<QGuiApplication*>(QCoreApplication::instance()) == nullptr)
    {
        return false;
    }
    const QStyleHints* hints = QGuiApplication::styleHints();
    return hints != nullptr && hints->accessibility()->motionPreference() == Qt::MotionPreference::ReducedMotion;
}

} // namespace snake::ui
