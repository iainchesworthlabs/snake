// SPDX-License-Identifier: GPL-3.0-or-later
#include "viewmodel/ui_events.hpp"

#include <QJSEngine>
#include <QQmlEngine>

namespace snake::ui
{

UiEvents& UiEvents::instance()
{
    static UiEvents events;
    return events;
}

UiEvents* UiEvents::create(QQmlEngine* /*engine*/, QJSEngine* /*script_engine*/)
{
    // The instance outlives every engine, so the engine must not take it over.
    QJSEngine::setObjectOwnership(&instance(), QJSEngine::CppOwnership);
    return &instance();
}

} // namespace snake::ui
