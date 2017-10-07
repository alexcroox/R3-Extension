#include "sql.h"

#include <chrono>

#include "extension.h"
#include "log.h"

#include "my_global.h"
#include "mysql.h"
#include "Poco/NumberParser.h"


namespace r3 {
    namespace sql {

        namespace {
            std::string host, database, user, password;
            uint32_t port;
            size_t timeout;
            MYSQL* connection;
            char escapeBuffer[701]; // vehicle_positions.cargo is the biggest string field with varchar(350). According to the docs of mysql_real_escape_string, we need length * 2 + 1 bytes for the buffer.
            std::mutex sessionMutex;
            std::atomic<bool> connected;
        }

        uint32_t parseUnsigned(const std::string& str) {
            uint32_t number = 0;
            if (!Poco::NumberParser::tryParseUnsigned(str, number)) {
                return 0;
            }
            return number;
        }

        double parseFloat(const std::string& str) {
            double number = 0;
            if (!Poco::NumberParser::tryParseFloat(str, number)) {
                return 0;
            }
            return number;
        }

        void escapeAndAddStringToQuery(const std::string& value, std::stringstream& query) {
            mysql_real_escape_string(connection, escapeBuffer, value.c_str(), value.length());
            query << "'" << escapeBuffer << "'";
        }

        void escapeAndAddStringToQueryWithComa(const std::string& value, std::stringstream& query) {
            escapeAndAddStringToQuery(value, query);
            query << ",";
        }

        bool executeMultiStatementQuery(const std::stringstream& query) {
            bool successfull = true;
            if (mysql_query(connection, query.str().c_str())) {
                successfull = false;
                log::logger->error("Error executing query! Error: '{}'", mysql_error(connection));
                log::logger->trace("Failed query: {}", query.str());
            }
            int status = -1;
            do {
                status = mysql_next_result(connection);
                if (status > 0) {
                    successfull = false;
                    log::logger->error("There was an error executing multiple queries! Status '{}', error: '{}'", status, mysql_error(connection));
                }
            } while (status == 0);
            return successfull;
        }

        bool initialize(const std::string& host_, uint32_t port_, const std::string& database_, const std::string& user_, const std::string& password_, size_t timeout_) {
            host = host_;
            port = port_;
            database = database_;
            user = user_;
            password = password_;
            timeout = timeout_;
            return true;
        }

        void finalize() {
            mysql_close(connection);
        }

        void run() {
            while (true) {
                std::vector<Request> requests;
                log::logger->trace("Popping requests from queue.");
                auto popStart = std::chrono::high_resolution_clock::now();
                extension::popAndFill(requests, MAX_PROCESS_REQUEST_COUNT);
                auto popEnd = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double> popDuration = popEnd - popStart;
                log::logger->trace("Popped '{}' requests from queue in '{}' seconds.", requests.size(), popDuration.count());
                auto lockStart = std::chrono::high_resolution_clock::now();
                std::lock_guard<std::mutex> lock(sessionMutex);
                auto lockEnd = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double> lockDuration = lockEnd - lockStart;
                log::logger->trace("Acquiring lock took '{}' seconds.", lockDuration.count());
                auto processRequestStart = std::chrono::high_resolution_clock::now();
                bool hasPoision = processRequests(requests);
                auto processRequestEnd = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double> processRequestDuration = processRequestEnd - processRequestStart;
                log::logger->trace("Processing '{}' requests took '{}' seconds.", requests.size(), processRequestDuration.count());
                if (hasPoision) { break; }
            }
        }

        std::mutex& getSessionMutex() {
            return sessionMutex;
        }

        bool isConnected() {
            return connected;
        }

        std::string connect() {
            if (connected) { return ""; }
            log::logger->info("Connecting to MySQL server at '{}@{}:{}/{}'.", user, host, port, database);
            connected = false;

            connection = mysql_init(nullptr);
            if (connection == nullptr) {
                std::string message = fmt::format("Initializing connection failed! Error: '{}'.", mysql_error(connection));
                log::logger->error(message);
                connected = false;
                return message;
            }

            auto connectResult = mysql_real_connect(connection, host.c_str(), user.c_str(), password.c_str(), database.c_str(), port,
                nullptr, CLIENT_COMPRESS | CLIENT_MULTI_STATEMENTS);
            if (connectResult == nullptr) {
                std::string message = fmt::format("Failed to connect to MySQL server! Error: '{}'", mysql_error(connection));
                log::logger->error(message);
                connected = false;
                return message;
            }
            connected = true;
            return "";
        }

        Response processCreateMissionRequest(const Request& request) {
            Response response{ RESPONSE_TYPE_OK, EMPTY_SQF_DATA };
            std::stringstream query;
            std::string missionName = request.params[0];
            std::string missionDisplayName = request.params[1];
            std::string terrain = request.params[2];
            std::string author = request.params[3];
            double dayTime = parseFloat(request.params[4]);
            std::string addonVersion = request.params[5];
            std::string fileName = request.params[6];
            log::logger->debug("Inserting into 'missions' values missionName '{}', missionDisplayName'{}', terrain '{}', author '{}' ,dayTime '{}', addonVersion '{}', fileName '{}'.",
                missionName, missionDisplayName, terrain, author, dayTime, addonVersion, fileName);
            query << "INSERT INTO missions(name, display_name, terrain, author, day_time, created_at, addon_version, file_name) VALUES(";
            escapeAndAddStringToQueryWithComa(missionName, query);
            escapeAndAddStringToQueryWithComa(missionDisplayName, query);
            escapeAndAddStringToQueryWithComa(terrain, query);
            escapeAndAddStringToQueryWithComa(author, query);
            query << dayTime << ",";
            query << "UTC_TIMESTAMP(),";
            escapeAndAddStringToQueryWithComa(addonVersion, query);
            escapeAndAddStringToQuery(fileName, query);
            query << ");";
            bool successfull = executeMultiStatementQuery(query);
            if (!successfull) {
                log::logger->error("Error creating mission!");
                response.type = RESPONSE_TYPE_ERROR;
                response.data = "\"Error creating mission!\"";
                return response;
            }

            auto replayId = mysql_insert_id(connection);
            log::logger->debug("New mission id is '{}'.", replayId);
            response.data = std::to_string(replayId);
            return response;
        }

        bool processRequests(const std::vector<Request>& requests) {
            if (requests.empty()) { return false; }
            bool hasPoison = false;
            std::stringstream query;
            for (const auto& request : requests) {
                hasPoison = hasPoison || request.command == REQUEST_COMMAND_POISON;
                auto paramsSize = request.params.size();
                log::logger->trace("Request command '{}' params size '{}'!", request.command, request.params.size());
                if (request.command == "update_mission" && paramsSize == 2) {
                    uint32_t replayId = parseUnsigned(request.params[0]);
                    uint32_t missionTime = parseUnsigned(request.params[1]);
                    log::logger->debug("Updating 'missions' values last_mission_time '{}', id '{}'.", missionTime, replayId);
                    query << fmt::format("UPDATE missions SET last_event_time = UTC_TIMESTAMP(), last_mission_time = {} WHERE id = {} LIMIT 1;", missionTime, replayId);
                }
                else if (request.command == "infantry" && paramsSize == 13) {
                    uint32_t replayId = parseUnsigned(request.params[0]);
                    std::string playerId = request.params[1];
                    uint32_t entityId = parseUnsigned(request.params[2]);
                    std::string unitName = request.params[3];
                    uint32_t unitFaction = parseUnsigned(request.params[4]);
                    std::string unitClass = request.params[5];
                    std::string unitGroupId = request.params[6];
                    uint32_t unitIsLeader = parseUnsigned(request.params[7]);
                    std::string unitIcon = request.params[8];
                    std::string unitWeapon = request.params[9];
                    std::string unitLauncher = request.params[10];
                    std::string unitData = request.params[11];
                    uint32_t missionTime = parseUnsigned(request.params[12]);
                    log::logger->debug("Inserting into 'infantry' values mission '{}', playerId '{}', entityId '{}', name '{}', faction '{}', class '{}', group '{}', leader '{}', icon '{}', weapon '{}', launcher '{}', data '{}', mission_time '{}'.",
                        replayId, playerId, entityId, unitName, unitFaction, unitClass, unitGroupId, unitIsLeader, unitIcon, unitWeapon, unitLauncher, unitData, missionTime);
                    query << "INSERT INTO infantry(mission, player_id, entity_id, name, faction, class, `group`, leader, icon, weapon, launcher, data, mission_time) VALUES (";
                    query << replayId << ",";
                    escapeAndAddStringToQueryWithComa(playerId, query);
                    query << entityId << ",";
                    escapeAndAddStringToQueryWithComa(unitName, query);
                    query << unitFaction << ",";
                    escapeAndAddStringToQueryWithComa(unitClass, query);
                    escapeAndAddStringToQueryWithComa(unitGroupId, query);
                    query << unitIsLeader << ",";
                    escapeAndAddStringToQueryWithComa(unitIcon, query);
                    escapeAndAddStringToQueryWithComa(unitWeapon, query);
                    escapeAndAddStringToQueryWithComa(unitLauncher, query);
                    escapeAndAddStringToQueryWithComa(unitData, query);
                    query << missionTime << ");";
                }
                else if (request.command == "infantry_positions" && paramsSize == 8) {
                    uint32_t replayId = parseUnsigned(request.params[0]);
                    uint32_t entityId = parseUnsigned(request.params[1]);
                    double posX = parseFloat(request.params[2]);
                    double posY = parseFloat(request.params[3]);
                    uint32_t direction = parseUnsigned(request.params[4]);
                    uint32_t keyFrame = parseUnsigned(request.params[5]);
                    uint32_t isDead = parseUnsigned(request.params[6]);
                    uint32_t missionTime = parseUnsigned(request.params[7]);
                    log::logger->debug("Inserting into 'infantry_positions' values mission '{}', entity_id '{}', x '{}', y '{}', direction '{}', key_frame '{}', is_dead '{}', mission_time '{}'.",
                        replayId, entityId, posX, posY, direction, keyFrame, isDead, missionTime);
                    query << "INSERT INTO infantry_positions(mission, entity_id, x, y, direction, key_frame, is_dead, mission_time) VALUES (";
                    query << replayId << ",";
                    query << entityId << ",";
                    query << posX << ",";
                    query << posY << ",";
                    query << direction << ",";
                    query << keyFrame << ",";
                    query << isDead << ",";
                    query << missionTime << ");";
                }
                else if (request.command == "vehicles" && paramsSize == 6) {
                    uint32_t replayId = parseUnsigned(request.params[0]);
                    uint32_t entityId = parseUnsigned(request.params[1]);
                    std::string vehicleClass = request.params[2];
                    std::string vehicleIcon = request.params[3];
                    std::string vehicleIconPath = request.params[4];
                    uint32_t missionTime = parseUnsigned(request.params[5]);
                    log::logger->debug("Inserting into 'vehicles' values mission '{}', entity_id '{}', class '{}', icon '{}', icon_path '{}', mission_time '{}'.",
                        replayId, entityId, vehicleClass, vehicleIcon, vehicleIconPath, missionTime);
                    query << "INSERT INTO vehicles(mission, entity_id, class, icon, icon_path, mission_time) VALUES (";
                    query << replayId << ",";
                    query << entityId << ",";
                    escapeAndAddStringToQueryWithComa(vehicleClass, query);
                    escapeAndAddStringToQueryWithComa(vehicleIcon, query);
                    escapeAndAddStringToQueryWithComa(vehicleIconPath, query);
                    query << missionTime << ");";
                }
                else if (request.command == "vehicle_positions" && paramsSize == 12) {
                    uint32_t replayId = parseUnsigned(request.params[0]);
                    uint32_t entityId = parseUnsigned(request.params[1]);
                    double posX = parseFloat(request.params[2]);
                    double posY = parseFloat(request.params[3]);
                    double posZ = parseFloat(request.params[4]);
                    uint32_t direction = parseUnsigned(request.params[5]);
                    uint32_t keyFrame = parseUnsigned(request.params[6]);
                    std::string driver = request.params[7];
                    std::string crew = request.params[8];
                    std::string cargo = request.params[9];
                    uint32_t isDead = parseUnsigned(request.params[10]);
                    uint32_t missionTime = parseUnsigned(request.params[11]);
                    log::logger->debug("Inserting into 'vehicle_positions' values mission '{}', entity_id '{}', x '{}', y '{}', z '{}', direction '{}', key_frame '{}', driver '{}', crew '{}', cargo '{}', is_dead '{}', mission_time '{}'.",
                        replayId, entityId, posX, posY, posZ, direction, keyFrame, driver, crew, cargo, isDead, missionTime);
                    query << "INSERT INTO vehicle_positions(mission, entity_id, x, y, z, direction, key_frame, driver, crew, cargo, is_dead, mission_time) VALUES (";
                    query << replayId << ",";
                    query << entityId << ",";
                    query << posX << ",";
                    query << posY << ",";
                    query << posZ << ",";
                    query << direction << ",";
                    query << keyFrame << ",";
                    escapeAndAddStringToQueryWithComa(driver, query);
                    escapeAndAddStringToQueryWithComa(crew, query);
                    escapeAndAddStringToQueryWithComa(cargo, query);
                    query << isDead << ",";
                    query << missionTime << ");";
                }
                else if (request.command == "events_connections" && paramsSize == 5) {
                    uint32_t replayId = parseUnsigned(request.params[0]);
                    uint32_t missionTime = parseUnsigned(request.params[1]);
                    std::string type = request.params[2];
                    std::string playerId = request.params[3];
                    std::string name = request.params[4];
                    log::logger->debug("Inserting into 'events_connections' values mission '{}', mission_time '{}', type '{}', player_id '{}', player_name '{}'.",
                        replayId, missionTime, type, playerId, name);
                    query << "INSERT INTO events_connections(mission, mission_time, type, player_id, player_name) VALUES (";
                    query << replayId << ",";
                    query << missionTime << ",";
                    escapeAndAddStringToQueryWithComa(type, query);
                    escapeAndAddStringToQueryWithComa(playerId, query);
                    escapeAndAddStringToQuery(name, query);
                    query << ");";
                }
                else if (request.command == "events_get_in_out" && paramsSize == 5) {
                    uint32_t replayId = parseUnsigned(request.params[0]);
                    uint32_t missionTime = parseUnsigned(request.params[1]);
                    std::string type = request.params[2];
                    uint32_t entityUnit = parseUnsigned(request.params[3]);
                    uint32_t entityVehicle = parseUnsigned(request.params[4]);
                    log::logger->debug("Inserting into 'events_get_in_out' values mission '{}', mission_time '{}', type '{}', entity_unit '{}', entity_vehicle '{}'.",
                        replayId, missionTime, type, entityUnit, entityVehicle);
                    query << "INSERT INTO events_get_in_out(mission, mission_time, type, entity_unit, entity_vehicle) VALUES (";
                    query << replayId << ",";
                    query << missionTime << ",";
                    escapeAndAddStringToQueryWithComa(type, query);
                    query << entityUnit << ",";
                    query << entityVehicle << ");";
                }
                else if (request.command == "events_projectile" && paramsSize == 7) {
                    uint32_t replayId = parseUnsigned(request.params[0]);
                    uint32_t missionTime = parseUnsigned(request.params[1]);
                    std::string grenadeType = request.params[2];
                    uint32_t entityAttacker = parseUnsigned(request.params[3]);
                    double posX = parseFloat(request.params[4]);
                    double posY = parseFloat(request.params[5]);
                    std::string projectileName = request.params[6];
                    log::logger->debug("Inserting into 'events_projectile' values mission '{}', mission_time '{}', type '{}', entity_attacker '{}', x '{}', y '{}', projectile_name '{}'.",
                        replayId, missionTime, grenadeType, entityAttacker, posX, posY, projectileName);
                    query << "INSERT INTO events_projectile(mission, mission_time, type, entity_attacker, x, y, projectile_name) VALUES (";
                    query << replayId << ",";
                    query << missionTime << ",";
                    escapeAndAddStringToQueryWithComa(grenadeType, query);
                    query << entityAttacker << ",";
                    query << posX << ",";
                    query << posY << ",";
                    escapeAndAddStringToQuery(projectileName, query);
                    query << ");";
                }
                else if (request.command == "events_downed" && paramsSize == 9) {
                    uint32_t replayId = parseUnsigned(request.params[0]);
                    uint32_t missionTime = parseUnsigned(request.params[1]);
                    std::string type = request.params[2];
                    uint32_t entityAttacker = parseUnsigned(request.params[3]);
                    uint32_t entityVictim = parseUnsigned(request.params[4]);
                    uint32_t attackerVehicle = parseUnsigned(request.params[5]);
                    uint32_t sameFaction = parseUnsigned(request.params[6]);
                    uint32_t attackerDistance = parseUnsigned(request.params[7]);
                    std::string weapon = request.params[8];
                    log::logger->debug("Inserting into 'events_downed' values mission '{}', mission_time '{}', type '{}', entity_attacker '{}', entity_victim '{}', attacker_vehicle '{}', same_faction '{}', distance '{}', weapon '{}'.",
                        replayId, missionTime, type, entityAttacker, entityVictim, attackerVehicle, sameFaction, attackerDistance, weapon);
                    query << "INSERT INTO events_downed(mission, mission_time, type, entity_attacker, entity_victim, attacker_vehicle, same_faction, distance, weapon) VALUES (";
                    query << replayId << ",";
                    query << missionTime << ",";
                    escapeAndAddStringToQueryWithComa(type, query);
                    query << entityAttacker << ",";
                    query << entityVictim << ",";
                    query << attackerVehicle << ",";
                    query << sameFaction << ",";
                    query << attackerDistance << ",";
                    escapeAndAddStringToQuery(weapon, query);
                    query << ");";
                }
                else if (request.command == "events_missile" && paramsSize == 6) {
                    uint32_t replayId = parseUnsigned(request.params[0]);
                    uint32_t missionTime = parseUnsigned(request.params[1]);
                    std::string type = request.params[2];
                    uint32_t entityAttacker = parseUnsigned(request.params[3]);
                    uint32_t entityVictim = parseUnsigned(request.params[4]);
                    std::string weapon = request.params[5];
                    log::logger->debug("Inserting into 'events_missile' values mission '{}', mission_time '{}', type '{}', entity_attacker '{}', entity_victim '{}', weapon '{}'.",
                        replayId, missionTime, type, entityAttacker, entityVictim, weapon);
                    query << "INSERT INTO events_missile(mission, mission_time, type, entity_attacker, entity_victim, weapon) VALUES (";
                    query << replayId << ",";
                    query << missionTime << ",";
                    escapeAndAddStringToQueryWithComa(type, query);
                    query << entityAttacker << ",";
                    query << entityVictim << ",";
                    escapeAndAddStringToQuery(weapon, query);
                    query << ");";
                }
                else {
                    log::logger->debug("Invlaid command type '{}'!", request.command);
                }
            }
            log::logger->trace("Multi statement query: {}", query.str()),
            executeMultiStatementQuery(query);
            return hasPoison;
        }

    } // namespace sql
} // namespace r3
