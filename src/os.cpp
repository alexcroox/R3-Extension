#include "os.h"

#include <fstream>

#ifdef _WIN32
#include "shlobj.h"
#else
#include <sys/stat.h>
#endif

namespace r3 {
namespace os {

    std::string getEnvironmentVariableValue(const std::string& name, const std::string& defaultValue) {
#ifdef _WIN32
        char* buffer = nullptr;
        _dupenv_s(&buffer, nullptr, name.c_str());
        std::string value = buffer == nullptr ? defaultValue : std::string(buffer);
        free(buffer);
        return value;
#else
        auto value = std::getenv(name.c_str());
        return value == nullptr ? defaultValue : std::string(value);
#endif
    }

    bool directoryExists(const std::string& directory) {
#ifdef _WIN32
        return GetFileAttributes(directory.c_str()) != INVALID_FILE_ATTRIBUTES;
#else
        struct stat info;
        stat(directory.c_str(), &info);
        return S_ISDIR(info.st_mode);
#endif
    }

    bool fileExists(const std::string& file) {
        return std::ifstream(file.c_str()).good();
    }

} // namespace os
} // namespace r3
