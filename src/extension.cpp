#include "extension.h"

#include <fstream>
#include <regex>

#ifdef _WIN32
#include "shlobj.h"
#endif

#include "log.hpp"
#include "os.h"
#include "config.h"

namespace r3 {
namespace extension {

namespace {
    const std::string EXTENSION_FOLDER_ENV_VAR = "R3_EXTENSION_HOME";
    const std::string EXTENSION_FOLDER = "R3Extension";
    const std::string CONFIG_FILE = "config.properties";

    Queue<Request> requests;
    std::thread sqlThread;
    std::string configError = "";
}

    void respond(char* output, const std::string& data) {
        data.copy(output, data.length());
        output[data.length()] = '\0';
    }

    std::string getExtensionFolder() {
        std::string extensionFolder = os::getEnvironmentVariableValue(EXTENSION_FOLDER_ENV_VAR, ".");
        if (extensionFolder == ".") {
#ifdef _WIN32
            PWSTR wpath = nullptr;
            if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &wpath))) {
                std::wstring wstringPath(wpath);
                extensionFolder = std::string(wstringPath.begin(), wstringPath.end());
            }
            CoTaskMemFree(wpath);
#else
            extensionFolder = os::getEnvironmentVariableValue("HOME", ".");
#endif
            extensionFolder += os::pathSeparator + EXTENSION_FOLDER;
        }
        return extensionFolder;
    }

    void stripDoubleQuotedParams(std::vector<std::string>& params) {
        for (auto&& param : params) {
            if (param.length() >= 2 && param.front() == '"' && param.back() == '"') {
                param.erase(0, 1);
                param.pop_back();
            }
        }
    }

    bool initialize() {
        std::string extensionFolder(getExtensionFolder());
        if (!os::directoryExists(extensionFolder)) {
            std::string message = fmt::format("Extension folder doesn't exist at '{}'!", extensionFolder);
            configError += message;
            return false;
        }

        std::string configFile(fmt::format("{}{}{}", extensionFolder, os::pathSeparator, CONFIG_FILE));
        if (!os::fileExists(configFile)) {
            std::string message = fmt::format("Config file is missing from '{}'!", configFile);
            configError += message;
            log::initialize(extensionFolder, "info");
            log::error(message);
            return false;
        }

        std::string errors = config::readConfigFile(configFile);
        if (!errors.empty()) {
            configError += errors;
            log::initialize(extensionFolder, "info");
            log::error(configError);
            return false;
        }

        std::string logLevel = config::getLogLevel();
        log::initialize(extensionFolder, logLevel);
        std::string host = config::getDbHost();
        uint32_t port = config::getDbPort();
        std::string database = config::getDbDatabase();
        std::string user = config::getDbUsername();
        std::string password = config::getDbPassword();
        sql::initialize(host, port, database, user, password);

        log::info("Starting r3_extension version '{}'.", R3_EXTENSION_VERSION);
        return true;
    }

    void finalize() {
        if (sql::isConnected()) {
            requests.push(Request{ REQUEST_COMMAND_POISON });
            sqlThread.join();
            sql::finalize();
        }
        log::info("Stopped r3_extension version '{}'.", R3_EXTENSION_VERSION);
    }

    int call(char *output, int outputSize, const char *function, const char **args, int argCount) {
        if (!configError.empty()) {
            respond(output, fmt::format("\"{}\"", configError));
            return RESPONSE_RETURN_CODE_ERROR;
        }
        Request request{ "" };
        request.command = std::string(function);
        request.params.insert(request.params.end(), args, args + argCount);
        stripDoubleQuotedParams(request.params);
        log::trace("Command '{}', params size '{}'.", request.command, request.params.size());
        if (request.command == "version") {
            respond(output, fmt::format("\"{}\"", R3_EXTENSION_VERSION));
            return RESPONSE_RETURN_CODE_OK;
        }
        else if (request.command == "connect") {
            if (sql::isConnected()) {
                respond(output, "true");
                return RESPONSE_RETURN_CODE_OK;
            }
            std::string message = sql::connect();
            if (message.empty()) {
                sqlThread = std::thread(sql::run);
                respond(output, "true");
                return RESPONSE_RETURN_CODE_OK;
            }
            respond(output, message);
            return RESPONSE_RETURN_CODE_ERROR;
        }
        else if (!sql::isConnected()) {
            respond(output, "\"Not connected to the database!\"");
            return RESPONSE_RETURN_CODE_ERROR;
        }
        else if (request.command == "create_mission" && request.params.size() == 7) {
            Response response;
            {
                std::lock_guard<std::mutex> lock(sql::getSessionMutex());
                response = sql::processCreateMissionRequest(request);
            }
            respond(output, response.data);
            return RESPONSE_RETURN_CODE_OK;
        }
        else if (
            request.command == "infantry" || 
            request.command == "infantry_positions" || 
            request.command == "vehicles" || 
            request.command == "vehicle_positions" || 
            request.command == "events_connections" || 
            request.command == "events_get_in_out" || 
            request.command == "events_projectile" || 
            request.command == "events_downed" || 
            request.command == "update_mission" || 
            request.command == "events_missile") {

            log::trace("Pushing request '{}' to queue .", request.command);
            requests.push(request);
            log::trace("Pushed request '{}' to queue .", request.command);
            respond(output, EMPTY_SQF_DATA);
            return RESPONSE_RETURN_CODE_OK;
        }
        respond(output, fmt::format("\"Unkown command '{}'\"", request.command));
        return RESPONSE_RETURN_CODE_ERROR;
    }

    Request popRequest() {
        return requests.pop();
    }

    void popAndFill(std::vector<Request>& target, size_t amount) {
        requests.popAndFill(target, amount);
    }

} // namespace extension
} // namespace r3
