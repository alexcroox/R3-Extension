#include "log.h"

#include <iomanip>
#include <ctime>
#include <sstream>

#include "Poco/Path.h"

namespace r3 {
namespace log {

namespace {
    const std::string LOGGER_NAME = "r3_extension_log";
}

    std::shared_ptr<spdlog::logger> logger;

    spdlog::level::level_enum getLogLevel(const std::string& logLevel) {
        if (logLevel == "debug") { return spdlog::level::debug; }
        if (logLevel == "trace") { return spdlog::level::trace; }
        return spdlog::level::info;
    }

    std::string getLogFileName() {
        auto time = std::time(nullptr);
        auto localTime = *std::localtime(&time);
        std::ostringstream fileName;
        fileName << LOGGER_NAME;
        fileName << std::put_time(&localTime, "_%Y-%m-%d_%H-%M-%S");
        return fileName.str();
    }

    bool initialze(const std::string& extensionFolder, const std::string& logLevel) {
        logger = spdlog::rotating_logger_mt(LOGGER_NAME, fmt::format("{}{}{}", extensionFolder, Poco::Path::separator(), getLogFileName()), 1024 * 1024 * 20, 1);
        logger->flush_on(spdlog::level::trace);
        logger->set_level(getLogLevel(logLevel));
        return true;
    }

    void finalize() {
    };

} // namespace log
} // namespace r3
