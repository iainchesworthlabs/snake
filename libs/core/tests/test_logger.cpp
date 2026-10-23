// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/logger.hpp"

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>
#include <thread>
#include <vector>

using snake::LogLevel;
using snake::StreamLogger;

TEST_CASE("Every level has its own prefix", "[logger]")
{
    // The old logger wrote [TRACE] for all six levels.
    CHECK(snake::level_prefix(LogLevel::TRACE) == "[TRACE] ");
    CHECK(snake::level_prefix(LogLevel::DEBUG) == "[DEBUG] ");
    CHECK(snake::level_prefix(LogLevel::INFO) == "[INFO] ");
    CHECK(snake::level_prefix(LogLevel::WARNING) == "[WARNING] ");
    CHECK(snake::level_prefix(LogLevel::ERROR) == "[ERROR] ");
    CHECK(snake::level_prefix(LogLevel::FATAL) == "[FATAL] ");
}

TEST_CASE("StreamLogger writes one prefixed line per message", "[logger]")
{
    std::ostringstream out;
    const StreamLogger logger(out, LogLevel::TRACE);

    logger.trace("a");
    logger.debug("b");
    logger.info("c");
    logger.warning("d");
    logger.error("e");
    logger.fatal("f");

    CHECK(out.str() == "[TRACE] a\n[DEBUG] b\n[INFO] c\n[WARNING] d\n[ERROR] e\n[FATAL] f\n");
}

TEST_CASE("StreamLogger drops messages below the minimum level", "[logger]")
{
    std::ostringstream out;
    const StreamLogger logger(out);
    REQUIRE(logger.minimum_level() == LogLevel::INFO);

    logger.trace("hidden");
    logger.debug("hidden");
    logger.info("shown");
    logger.error("shown too");

    CHECK(out.str() == "[INFO] shown\n[ERROR] shown too\n");
}

TEST_CASE("StreamLogger keeps UTF-8 text intact", "[logger]")
{
    std::ostringstream out;
    const StreamLogger logger(out);
    logger.info("蛇 and ß");
    CHECK(out.str() == "[INFO] 蛇 and ß\n");
}

TEST_CASE("StreamLogger can be shared between threads", "[logger]")
{
    std::ostringstream out;
    const StreamLogger logger(out);
    {
        std::vector<std::jthread> threads;
        for (int t = 0; t < 4; ++t)
        {
            threads.emplace_back(
                [&logger]
                {
                    for (int i = 0; i < 50; ++i)
                    {
                        logger.info("x");
                    }
                });
        }
    }
    std::istringstream in(out.str());
    std::string line;
    int lines = 0;
    while (std::getline(in, line))
    {
        CHECK(line == "[INFO] x");
        ++lines;
    }
    CHECK(lines == 200);
}
