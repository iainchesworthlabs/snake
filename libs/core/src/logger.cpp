// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/logger.hpp"

#include <utility>

namespace snake
{

std::string_view level_prefix(LogLevel level) noexcept
{
    switch (level)
    {
    case LogLevel::TRACE:
        return "[TRACE] ";
    case LogLevel::DEBUG:
        return "[DEBUG] ";
    case LogLevel::INFO:
        return "[INFO] ";
    case LogLevel::WARNING:
        return "[WARNING] ";
    case LogLevel::ERROR:
        return "[ERROR] ";
    case LogLevel::FATAL:
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
