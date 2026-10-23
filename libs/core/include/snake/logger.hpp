// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <mutex>
#include <ostream>
#include <string_view>

namespace snake
{

// The enumerators carry a prefix because ERROR is a macro in the Windows headers and DEBUG is often defined on the
// compiler command line.
enum class LogLevel
{
    LEVEL_TRACE,
    LEVEL_DEBUG,
    LEVEL_INFO,
    LEVEL_WARNING,
    LEVEL_ERROR,
    LEVEL_FATAL,
};

/// The line prefix of a level, for example "[WARNING] ".
[[nodiscard]] std::string_view level_prefix(LogLevel level) noexcept;

/// Where the game reports what it is doing. Messages are UTF-8.
class Logger
{
public:
    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    virtual ~Logger() = default;

    virtual void log(LogLevel level, std::string_view message) const = 0;

    void trace(std::string_view message) const
    {
        log(LogLevel::LEVEL_TRACE, message);
    }
    void debug(std::string_view message) const
    {
        log(LogLevel::LEVEL_DEBUG, message);
    }
    void info(std::string_view message) const
    {
        log(LogLevel::LEVEL_INFO, message);
    }
    void warning(std::string_view message) const
    {
        log(LogLevel::LEVEL_WARNING, message);
    }
    void error(std::string_view message) const
    {
        log(LogLevel::LEVEL_ERROR, message);
    }
    void fatal(std::string_view message) const
    {
        log(LogLevel::LEVEL_FATAL, message);
    }
};

/// Writes one line per message, "<prefix><message>", to a stream. Messages below the minimum level
/// are dropped. Safe to call from several threads.
class StreamLogger final : public Logger
{
public:
    explicit StreamLogger(std::ostream& stream, LogLevel minimum_level = LogLevel::LEVEL_INFO);

    void log(LogLevel level, std::string_view message) const override;

    [[nodiscard]] LogLevel minimum_level() const noexcept
    {
        return m_MinimumLevel;
    }

private:
    std::ostream& m_Stream;
    LogLevel m_MinimumLevel;
    mutable std::mutex m_Mutex;
};

} // namespace snake
