#ifndef LOGGER_H
#define LOGGER_H

#include "spdlog/spdlog.h"

namespace r3 {
namespace log {
    extern std::shared_ptr<spdlog::logger> logger;

    bool initialize(const std::string& extensionFolder, const std::string& logLevel);
    bool isInitialized();
    void finalize();

} // namespace log
} // namespace r3

#endif // LOGGER_H
