#include "log.h"
#include "os.h"

#include <iomanip>
#include <ctime>
#include <sstream>

namespace r3 {
namespace log {

namespace {
    const std::string LOGGER_NAME = "r3_extension_log";
    bool initialized = false;
}

    std::shared_ptr<spdlog::logger> logger;

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

    bool initialze(const std::string& extensionFolder, const std::string& logLevel) {
        logger = spdlog::rotating_logger_mt(LOGGER_NAME, fmt::format("{}{}{}", extensionFolder, os::pathSeparator, getLogFileName()), 1024 * 1024 * 20, 1);
        logger->flush_on(spdlog::level::trace);
        logger->set_level(getLogLevel(logLevel));
        initialized = true;
        return true;
    }

    bool isInitialized() {
        return initialized;
    }

    void finalize() {
    };

} // namespace log
} // namespace r3
