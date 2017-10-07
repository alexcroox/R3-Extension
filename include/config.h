#ifndef CONFIG_H
#define CONFIG_H

#include <string>

namespace r3 {
namespace config {

    bool initialize();
    void finalize();
    std::string readConfigFile(const std::string& configFile);
    std::string getLogLevel();
    std::string getDbHost();
    uint32_t getDbPort();
    std::string getDbDatabase();
    std::string getDbUsername();
    std::string getDbPassword();

    } // namespace config
} // namespace r3

#endif // CONFIG_H
