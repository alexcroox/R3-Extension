#ifndef LOGGER_HPP
#define LOGGER_HPP

#include "os.h"

#include <iomanip>
#include <ctime>
#include <sstream>

#include "spdlog/spdlog.h"

namespace r3 {
namespace log {

namespace {
    const std::string LOGGER_NAME = "r3_extension_log";
    std::shared_ptr<spdlog::logger> logger;
    bool initialized = false;

    spdlog::level::level_enum getLogLevel(const std::string& logLevel) {
        if (logLevel == "debug") { return spdlog::level::debug; }
        if (logLevel == "trace") { return spdlog::level::trace; }
        return spdlog::level::info;
    }

    std::string getLogFileName() {
        time_t time = std::time(nullptr);
        tm timeInfo;
#ifdef _WIN32
        localtime_s(&timeInfo, &time);
#else
        localtime_r(&time, &timeInfo);
#endif
        std::ostringstream fileName;
        fileName << LOGGER_NAME;
        fileName << std::put_time(&timeInfo, "_%Y-%m-%d_%H-%M-%S");
        return fileName.str();
    }
}

    inline bool initialize(const std::string& extensionFolder, const std::string& logLevel) {
        logger = spdlog::rotating_logger_mt(LOGGER_NAME, fmt::format("{}{}{}", extensionFolder, os::pathSeparator, getLogFileName()), 1024 * 1024 * 20, 1);
        logger->flush_on(spdlog::level::trace);
        logger->set_level(getLogLevel(logLevel));
        initialized = true;
        return true;
    }

    template <typename... Args> inline void trace(const char* fmt, const Args&... args) {
        if (initialized) { logger->trace(fmt, args...); }
    }

    template <typename... Args> inline void debug(const char* fmt, const Args&... args) {
        if (initialized) { logger->debug(fmt, args...); }
    }

    template <typename... Args> inline void info(const char* fmt, const Args&... args) {
        if (initialized) { logger->info(fmt, args...); }
    }

    template <typename... Args> inline void warn(const char* fmt, const Args&... args) {
        if (initialized) { logger->warn(fmt, args...); }
    }

    template <typename... Args> inline void error(const char* fmt, const Args&... args) {
        if (initialized) { logger->error(fmt, args...); }
    }

    template <typename T> inline void error(const T& msg) {
        if (initialized) { logger->error(msg); }
    }

} // namespace log
} // namespace r3


#endif // LOGGER_HPP
