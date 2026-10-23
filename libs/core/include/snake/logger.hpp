// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <mutex>
#include <ostream>
#include <string_view>

namespace snake
{

enum class LogLevel
{
    TRACE,
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    FATAL,
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
        log(LogLevel::TRACE, message);
    }
    void debug(std::string_view message) const
    {
        log(LogLevel::DEBUG, message);
    }
    void info(std::string_view message) const
    {
        log(LogLevel::INFO, message);
    }
    void warning(std::string_view message) const
    {
        log(LogLevel::WARNING, message);
    }
    void error(std::string_view message) const
    {
        log(LogLevel::ERROR, message);
    }
    void fatal(std::string_view message) const
    {
        log(LogLevel::FATAL, message);
    }
};

/// Writes one line per message, "<prefix><message>", to a stream. Messages below the minimum level
/// are dropped. Safe to call from several threads.
class StreamLogger final : public Logger
{
public:
    explicit StreamLogger(std::ostream& stream, LogLevel minimum_level = LogLevel::INFO);

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
