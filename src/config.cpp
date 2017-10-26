#include "config.h"
#include "log.h"

#include <fstream>
#include <sstream>

namespace r3 {
namespace config {

namespace {
    std::string logLevel = "info";
    std::string dbHost = "";
    uint32_t dbPort = 0;
    std::string dbDatabase = "";
    std::string dbUsername = "";
    std::string dbPassword = "";
}

    std::string getConfigValue(std::unordered_map<std::string, std::string>& configs, const std::string& key, std::string& configErrors) {
        if (configs.find(key) == configs.end()) {
            configErrors += fmt::format(" Config file is missing key '{}'.", key);
            return "";
        }
        return configs[key];
    }

    void processConfigs(std::unordered_map<std::string, std::string>& configs, std::string& configErrors) {
        logLevel = getConfigValue(configs, "r3.log.level", configErrors);
        dbHost = getConfigValue(configs, "r3.db.host", configErrors);
        std::string portStr = getConfigValue(configs, "r3.db.port", configErrors);
        dbPort = std::strtoul(portStr.c_str(), nullptr, 10);
        dbDatabase = getConfigValue(configs, "r3.db.database", configErrors);
        dbUsername = getConfigValue(configs, "r3.db.username", configErrors);
        dbPassword = getConfigValue(configs, "r3.db.password", configErrors);
    }

    void trim(std::string& str) {
        std::stringstream trimmer;
        trimmer << str;
        str.clear();
        trimmer >> str;
    }

    bool initialize() {
        return true;
    }

    void finalize() {
    }

    std::string readConfigFile(const std::string& configFile) {
        std::string configErrors;
        std::unordered_map<std::string, std::string> configs;
        std::ifstream configFileStream(configFile);
        std::string line;
        while (std::getline(configFileStream, line)) {
            if (line[0] == '#') { continue; }
            std::istringstream lineStream(line);
            std::string key;
            if (std::getline(lineStream, key, '=')) {
                std::string value;
                if (std::getline(lineStream, value)) {
                    trim(key);
                    trim(value);
                    configs[key] = value;
                }
            }
        }
        processConfigs(configs, configErrors);
        return configErrors;
    }

    std::string getLogLevel() {
        return logLevel;
    }

    std::string getDbHost() {
        return dbHost;
    }

     uint32_t getDbPort() {
        return dbPort;
    }

    std::string getDbDatabase() {
        return dbDatabase;
    }

    std::string getDbUsername() {
        return dbUsername;
    }

    std::string getDbPassword() {
        return dbPassword;
    }

} // namespace config
} // namespace r3
