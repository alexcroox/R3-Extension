#ifndef OS_H
#define OS_H

#include <string>

namespace r3 {
namespace os {

#ifdef _WIN32
    const std::string pathSeparator = "\\";
#else
    const std::string pathSeparator = "/";
#endif

    std::string getEnvironmentVariableValue(const std::string& name, const std::string& defaultValue = "");
    bool directoryExists(const std::string& directory);
    bool fileExists(const std::string& file);

} // namespace os
} // namespace r3

#endif // OS
