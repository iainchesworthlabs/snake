// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/logger.hpp"

#include <utility>

namespace snake
{

std::string_view level_prefix(LogLevel level) noexcept
{
    switch (level)
    {
    case LogLevel::LEVEL_TRACE:
        return "[TRACE] ";
    case LogLevel::LEVEL_DEBUG:
        return "[DEBUG] ";
    case LogLevel::LEVEL_INFO:
        return "[INFO] ";
    case LogLevel::LEVEL_WARNING:
        return "[WARNING] ";
    case LogLevel::LEVEL_ERROR:
        return "[ERROR] ";
    case LogLevel::LEVEL_FATAL:
        return "[FATAL] ";
    }
    std::unreachable();
}

StreamLogger::StreamLogger(std::ostream& stream, LogLevel minimum_level)
    : m_Stream(stream), m_MinimumLevel(minimum_level)
{
}

void StreamLogger::log(LogLevel level, std::string_view message) const
{
    if (level < m_MinimumLevel)
    {
        return;
    }
    const std::lock_guard lock(m_Mutex);
    m_Stream << level_prefix(level) << message << '\n';
}

} // namespace snake
