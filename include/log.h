#ifndef LOGGER_HPP
#define LOGGER_HPP

#include "os.h"

#include "spdlog/spdlog.h"

namespace r3 {
namespace log {
    extern std::shared_ptr<spdlog::logger> logger;

    std::string initialize(const std::string& extensionFolder, const std::string& logLevel);
    void setLogLevel(const std::string& logLevel);
    bool isInitialized();

    template <typename... Args> void trace(const char* fmt, const Args&... args) {
        if (isInitialized()) { logger->trace(fmt, args...); }
    }

    template <typename... Args> void debug(const char* fmt, const Args&... args) {
        if (isInitialized()) { logger->debug(fmt, args...); }
    }

    template <typename... Args> void info(const char* fmt, const Args&... args) {
        if (isInitialized()) { logger->info(fmt, args...); }
    }

    template <typename... Args> void warn(const char* fmt, const Args&... args) {
        if (isInitialized()) { logger->warn(fmt, args...); }
    }

    template <typename... Args> void error(const char* fmt, const Args&... args) {
        if (isInitialized()) { logger->error(fmt, args...); }
    }

    template <typename T> void error(const T& msg) {
        if (isInitialized()) { logger->error(msg); }
    }

} // namespace log
} // namespace r3

#endif // LOGGER_HPP
