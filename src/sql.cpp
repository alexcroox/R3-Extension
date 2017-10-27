#include "sql.h"

#include <chrono>

#include "extension.h"
#include "log.h"

#include "my_global.h"
#include "mysql.h"

#define ESCAPE_BUFFER_MAX_STRING_LENGTH 350

namespace r3 {
namespace sql {

namespace {
    MYSQL* connection;
    char escapeBuffer[ESCAPE_BUFFER_MAX_STRING_LENGTH * 2 + 1]; // vehicle_positions.cargo is the biggest string field with varchar(350). According to the docs of mysql_real_escape_string, we need length * 2 + 1 bytes for the buffer.
    std::mutex sessionMutex;
    std::atomic<bool> connected;
}

    // This will overflow without any errors when the parsed string is bigger than uint32_t. Returns 0 on parse error.
    uint32_t parseUnsigned(const std::string& str) {
        return std::strtoul(str.c_str(), nullptr, 10);
    }

    // Returns 0 on parse error. Also can cause problems if the current C locale doesn't use dot as the decimal point.
    double parseFloat(const std::string& str) {
        return std::strtod(str.c_str(), nullptr);
    }

    void escapeAndAddStringToQuery(const std::string& value, std::ostringstream& query) {
        if (value.length() > ESCAPE_BUFFER_MAX_STRING_LENGTH) {
            log::warn("String '{}' is too long to escape! Extension only supports strings for up to '{}' characters!", value, ESCAPE_BUFFER_MAX_STRING_LENGTH);
            query << "''";
            return;
        }
        mysql_real_escape_string(connection, escapeBuffer, value.c_str(), value.length());
        query << "'" << escapeBuffer << "'";
    }

    void escapeAndAddStringToQueryWithComa(const std::string& value, std::ostringstream& query) {
        escapeAndAddStringToQuery(value, query);
        query << ",";
    }

    bool executeMultiStatementQuery(const std::ostringstream& query) {
        bool successfull = true;
        if (mysql_query(connection, query.str().c_str())) {
            successfull = false;
            log::error("Error executing query! Error: '{}'", mysql_error(connection));
            log::trace("Failed query: {}", query.str());
        }
        int status = -1;
        do {
            status = mysql_next_result(connection);
            if (status > 0) {
                successfull = false;
                log::error("There was an error executing multiple queries! Status '{}', error: '{}'", status, mysql_error(connection));
            }
        } while (status == 0);
        return successfull;
    }

    void tryConcatenateQueries(std::ostringstream& query, std::ostringstream& valuesQueryFragment, const std::string& insertQueryFragement) {
        std::string valuesQueryFragmentString = valuesQueryFragment.str();
        if (!valuesQueryFragmentString.empty()) {
            valuesQueryFragmentString[valuesQueryFragmentString.length() - 1] = ';';
            query << insertQueryFragement << valuesQueryFragmentString;
        }
    }

    void processInfantryCommand(std::ostringstream& query, const std::vector<std::string>& params) {
        uint32_t replayId = parseUnsigned(params[0]);
        std::string playerId = params[1];
        uint32_t entityId = parseUnsigned(params[2]);
        std::string unitName = params[3];
        uint32_t unitFaction = parseUnsigned(params[4]);
        std::string unitClass = params[5];
        std::string unitGroupId = params[6];
        uint32_t unitIsLeader = parseUnsigned(params[7]);
        std::string unitIcon = params[8];
        std::string unitWeapon = params[9];
        std::string unitLauncher = params[10];
        std::string unitData = params[11];
        uint32_t missionTime = parseUnsigned(params[12]);
        log::debug("Inserting into 'infantry' values mission '{}', playerId '{}', entityId '{}', name '{}', faction '{}', class '{}', group '{}', leader '{}', icon '{}', weapon '{}', launcher '{}', data '{}', mission_time '{}'.",
            replayId, playerId, entityId, unitName, unitFaction, unitClass, unitGroupId, unitIsLeader, unitIcon, unitWeapon, unitLauncher, unitData, missionTime);
        query << "(";
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
        query << missionTime << ")";
        query << " ON DUPLICATE KEY UPDATE player_id = ";
        escapeAndAddStringToQuery(playerId, query);
        query << ", name = ";
        escapeAndAddStringToQuery(unitName, query);
        query << ";";
    }

    void processInfantryPositionsCommand(std::ostringstream& query, const std::vector<std::string>& params) {
        uint32_t replayId = parseUnsigned(params[0]);
        uint32_t entityId = parseUnsigned(params[1]);
        double posX = parseFloat(params[2]);
        double posY = parseFloat(params[3]);
        uint32_t direction = parseUnsigned(params[4]);
        uint32_t keyFrame = parseUnsigned(params[5]);
        uint32_t isDead = parseUnsigned(params[6]);
        uint32_t missionTime = parseUnsigned(params[7]);
        log::debug("Inserting into 'infantry_positions' values mission '{}', entity_id '{}', x '{}', y '{}', direction '{}', key_frame '{}', is_dead '{}', mission_time '{}'.",
            replayId, entityId, posX, posY, direction, keyFrame, isDead, missionTime);
        query << "(";
        query << replayId << ",";
        query << entityId << ",";
        query << posX << ",";
        query << posY << ",";
        query << direction << ",";
        query << keyFrame << ",";
        query << isDead << ",";
        query << missionTime << "),";
    }

    void processVehiclesCommand(std::ostringstream& query, const std::vector<std::string>& params) {
        uint32_t replayId = parseUnsigned(params[0]);
        uint32_t entityId = parseUnsigned(params[1]);
        std::string vehicleClass = params[2];
        std::string vehicleIcon = params[3];
        std::string vehicleIconPath = params[4];
        uint32_t missionTime = parseUnsigned(params[5]);
        log::debug("Inserting into 'vehicles' values mission '{}', entity_id '{}', class '{}', icon '{}', icon_path '{}', mission_time '{}'.",
            replayId, entityId, vehicleClass, vehicleIcon, vehicleIconPath, missionTime);
        query << "(";
        query << replayId << ",";
        query << entityId << ",";
        escapeAndAddStringToQueryWithComa(vehicleClass, query);
        escapeAndAddStringToQueryWithComa(vehicleIcon, query);
        escapeAndAddStringToQueryWithComa(vehicleIconPath, query);
        query << missionTime << "),";
    }

    void processVehiclePositionsCommand(std::ostringstream& query, const std::vector<std::string>& params) {
        uint32_t replayId = parseUnsigned(params[0]);
        uint32_t entityId = parseUnsigned(params[1]);
        double posX = parseFloat(params[2]);
        double posY = parseFloat(params[3]);
        double posZ = parseFloat(params[4]);
        uint32_t direction = parseUnsigned(params[5]);
        uint32_t keyFrame = parseUnsigned(params[6]);
        std::string driver = params[7];
        std::string crew = params[8];
        std::string cargo = params[9];
        uint32_t isDead = parseUnsigned(params[10]);
        uint32_t missionTime = parseUnsigned(params[11]);
        log::debug("Inserting into 'vehicle_positions' values mission '{}', entity_id '{}', x '{}', y '{}', z '{}', direction '{}', key_frame '{}', driver '{}', crew '{}', cargo '{}', is_dead '{}', mission_time '{}'.",
            replayId, entityId, posX, posY, posZ, direction, keyFrame, driver, crew, cargo, isDead, missionTime);
        query << "(";
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
        query << missionTime << "),";
    }

    void processEventsConnectionsCommand(std::ostringstream& query, const std::vector<std::string>& params) {
        uint32_t replayId = parseUnsigned(params[0]);
        uint32_t missionTime = parseUnsigned(params[1]);
        std::string type = params[2];
        std::string playerId = params[3];
        std::string name = params[4];
        log::debug("Inserting into 'events_connections' values mission '{}', mission_time '{}', type '{}', player_id '{}', player_name '{}'.",
            replayId, missionTime, type, playerId, name);
        query << "(";
        query << replayId << ",";
        query << missionTime << ",";
        escapeAndAddStringToQueryWithComa(type, query);
        escapeAndAddStringToQueryWithComa(playerId, query);
        escapeAndAddStringToQuery(name, query);
        query << "),";
    }

    void processEventsGetInOutCommand(std::ostringstream& query, const std::vector<std::string>& params) {
        uint32_t replayId = parseUnsigned(params[0]);
        uint32_t missionTime = parseUnsigned(params[1]);
        std::string type = params[2];
        uint32_t entityUnit = parseUnsigned(params[3]);
        uint32_t entityVehicle = parseUnsigned(params[4]);
        log::debug("Inserting into 'events_get_in_out' values mission '{}', mission_time '{}', type '{}', entity_unit '{}', entity_vehicle '{}'.",
            replayId, missionTime, type, entityUnit, entityVehicle);
        query << "(";
        query << replayId << ",";
        query << missionTime << ",";
        escapeAndAddStringToQueryWithComa(type, query);
        query << entityUnit << ",";
        query << entityVehicle << "),";
    }

    void processEventsProjectileCommand(std::ostringstream& query, const std::vector<std::string>& params) {
        uint32_t replayId = parseUnsigned(params[0]);
        uint32_t missionTime = parseUnsigned(params[1]);
        std::string grenadeType = params[2];
        uint32_t entityAttacker = parseUnsigned(params[3]);
        double posX = parseFloat(params[4]);
        double posY = parseFloat(params[5]);
        std::string projectileName = params[6];
        log::debug("Inserting into 'events_projectile' values mission '{}', mission_time '{}', type '{}', entity_attacker '{}', x '{}', y '{}', projectile_name '{}'.",
            replayId, missionTime, grenadeType, entityAttacker, posX, posY, projectileName);
        query << "(";
        query << replayId << ",";
        query << missionTime << ",";
        escapeAndAddStringToQueryWithComa(grenadeType, query);
        query << entityAttacker << ",";
        query << posX << ",";
        query << posY << ",";
        escapeAndAddStringToQuery(projectileName, query);
        query << "),";
    }

    void processEventsDownedCommand(std::ostringstream& query, const std::vector<std::string>& params) {
        uint32_t replayId = parseUnsigned(params[0]);
        uint32_t missionTime = parseUnsigned(params[1]);
        std::string type = params[2];
        uint32_t entityAttacker = parseUnsigned(params[3]);
        uint32_t entityVictim = parseUnsigned(params[4]);
        uint32_t attackerVehicle = parseUnsigned(params[5]);
        uint32_t sameFaction = parseUnsigned(params[6]);
        uint32_t attackerDistance = parseUnsigned(params[7]);
        std::string weapon = params[8];
        log::debug("Inserting into 'events_downed' values mission '{}', mission_time '{}', type '{}', entity_attacker '{}', entity_victim '{}', attacker_vehicle '{}', same_faction '{}', distance '{}', weapon '{}'.",
            replayId, missionTime, type, entityAttacker, entityVictim, attackerVehicle, sameFaction, attackerDistance, weapon);
        query << "(";
        query << replayId << ",";
        query << missionTime << ",";
        escapeAndAddStringToQueryWithComa(type, query);
        query << entityAttacker << ",";
        query << entityVictim << ",";
        query << attackerVehicle << ",";
        query << sameFaction << ",";
        query << attackerDistance << ",";
        escapeAndAddStringToQuery(weapon, query);
        query << "),";
    }

    void processEventsMissileCommand(std::ostringstream& query, const std::vector<std::string>& params) {
        uint32_t replayId = parseUnsigned(params[0]);
        uint32_t missionTime = parseUnsigned(params[1]);
        std::string type = params[2];
        uint32_t entityAttacker = parseUnsigned(params[3]);
        uint32_t entityVictim = parseUnsigned(params[4]);
        std::string weapon = params[5];
        log::debug("Inserting into 'events_missile' values mission '{}', mission_time '{}', type '{}', entity_attacker '{}', entity_victim '{}', weapon '{}'.",
            replayId, missionTime, type, entityAttacker, entityVictim, weapon);
        query << "(";
        query << replayId << ",";
        query << missionTime << ",";
        escapeAndAddStringToQueryWithComa(type, query);
        query << entityAttacker << ",";
        query << entityVictim << ",";
        escapeAndAddStringToQuery(weapon, query);
        query << "),";
    }

    void finalize() {
        mysql_close(connection);
    }

    void run() {
        while (true) {
            std::vector<Request> requests;
            log::trace("Popping requests from queue.");
            auto popStart = std::chrono::high_resolution_clock::now();
            extension::popAndFill(requests, MAX_PROCESS_REQUEST_COUNT);
            auto popEnd = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> popDuration = popEnd - popStart;
            log::trace("Popped '{}' requests from queue in '{}' seconds.", requests.size(), popDuration.count());
            auto lockStart = std::chrono::high_resolution_clock::now();
            std::lock_guard<std::mutex> lock(sessionMutex);
            auto lockEnd = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> lockDuration = lockEnd - lockStart;
            log::trace("Acquiring lock took '{}' seconds.", lockDuration.count());
            auto processRequestStart = std::chrono::high_resolution_clock::now();
            bool hasPoision = processRequests(requests);
            auto processRequestEnd = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> processRequestDuration = processRequestEnd - processRequestStart;
            log::trace("Processing '{}' requests took '{}' seconds.", requests.size(), processRequestDuration.count());
            if (hasPoision) { break; }
        }
    }

    std::mutex& getSessionMutex() {
        return sessionMutex;
    }

    bool isConnected() {
        return connected;
    }

    std::string connect(const std::string& host, uint32_t port, const std::string& database, const std::string& user, const std::string& password) {
        if (connected) { return ""; }
        log::info("Connecting to MySQL server at '{}@{}:{}/{}'.", user, host, port, database);
        connected = false;

        connection = mysql_init(nullptr);
        if (connection == nullptr) {
            std::string message = fmt::format("Initializing connection failed! Error: '{}'.", mysql_error(connection));
            log::error(message);
            connected = false;
            return message;
        }

        auto connectResult = mysql_real_connect(connection, host.c_str(), user.c_str(), password.c_str(), database.c_str(), port,
            nullptr, CLIENT_COMPRESS | CLIENT_MULTI_STATEMENTS);
        if (connectResult == nullptr) {
            std::string message = fmt::format("Failed to connect to MySQL server! Error: '{}'", mysql_error(connection));
            log::error(message);
            connected = false;
            return message;
        }
        connected = true;
        return "";
    }

    Response processCreateMissionRequest(const Request& request) {
        Response response{ RESPONSE_TYPE_OK, EMPTY_SQF_DATA };
        std::ostringstream query;
        std::string missionName = request.params[0];
        std::string missionDisplayName = request.params[1];
        std::string terrain = request.params[2];
        std::string author = request.params[3];
        double dayTime = parseFloat(request.params[4]);
        std::string addonVersion = request.params[5];
        std::string fileName = request.params[6];
        log::debug("Inserting into 'missions' values missionName '{}', missionDisplayName'{}', terrain '{}', author '{}' ,dayTime '{}', addonVersion '{}', fileName '{}'.",
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
            log::error("Error creating mission!");
            response.type = RESPONSE_TYPE_ERROR;
            response.data = "\"Error creating mission!\"";
            return response;
        }

        auto replayId = mysql_insert_id(connection);
        log::debug("New mission id is '{}'.", replayId);
        response.data = std::to_string(replayId);
        return response;
    }

    bool processRequests(const std::vector<Request>& requests) {
        if (requests.empty()) { return false; }
        bool hasPoison = false;
        std::ostringstream query, infantryQuery, infantryPositionsQuery, vehiclesQuery,
            vehiclePositionsQuery, eventsConnectionsQuery, eventsGetInOutQuery,
            eventsProjectileQuery, eventsDownedQuery, eventsMissileQuery;
        for (const auto& request : requests) {
            hasPoison = hasPoison || request.command == REQUEST_COMMAND_POISON;
            auto paramsSize = request.params.size();
            log::trace("Request command '{}' params size '{}'!", request.command, request.params.size());
            if (request.command == "update_mission" && paramsSize == 2) {
                uint32_t replayId = parseUnsigned(request.params[0]);
                uint32_t missionTime = parseUnsigned(request.params[1]);
                log::debug("Updating 'missions' values last_mission_time '{}', id '{}'.", missionTime, replayId);
                query << fmt::format("UPDATE missions SET last_event_time = UTC_TIMESTAMP(), last_mission_time = {} WHERE id = {} LIMIT 1;", missionTime, replayId);
            }
            else if (request.command == "infantry" && paramsSize == 13) {
                processInfantryCommand(infantryQuery, request.params);
            }
            else if (request.command == "infantry_positions" && paramsSize == 8) {
                processInfantryPositionsCommand(infantryPositionsQuery, request.params);
            }
            else if (request.command == "vehicles" && paramsSize == 6) {
                processVehiclesCommand(vehiclesQuery, request.params);
            }
            else if (request.command == "vehicle_positions" && paramsSize == 12) {
                processVehiclePositionsCommand(vehiclePositionsQuery, request.params);
            }
            else if (request.command == "events_connections" && paramsSize == 5) {
                processEventsConnectionsCommand(eventsConnectionsQuery, request.params);
            }
            else if (request.command == "events_get_in_out" && paramsSize == 5) {
                processEventsGetInOutCommand(eventsGetInOutQuery, request.params);
            }
            else if (request.command == "events_projectile" && paramsSize == 7) {
                processEventsProjectileCommand(eventsProjectileQuery, request.params);
            }
            else if (request.command == "events_downed" && paramsSize == 9) {
                processEventsDownedCommand(eventsDownedQuery, request.params);
            }
            else if (request.command == "events_missile" && paramsSize == 6) {
                processEventsMissileCommand(eventsMissileQuery, request.params);
            }
            else {
                log::debug("Invlaid command type '{}' with param size '{}'!", request.command, request.params.size());
            }
        }
        tryConcatenateQueries(query, infantryQuery, "INSERT INTO infantry(mission, player_id, entity_id, name, faction, class, `group`, leader, icon, weapon, launcher, data, mission_time) VALUES ");
        tryConcatenateQueries(query, infantryPositionsQuery, "INSERT INTO infantry_positions(mission, entity_id, x, y, direction, key_frame, is_dead, mission_time) VALUES ");
        tryConcatenateQueries(query, vehiclesQuery, "INSERT INTO vehicles(mission, entity_id, class, icon, icon_path, mission_time) VALUES ");
        tryConcatenateQueries(query, vehiclePositionsQuery, "INSERT INTO vehicle_positions(mission, entity_id, x, y, z, direction, key_frame, driver, crew, cargo, is_dead, mission_time) VALUES ");
        tryConcatenateQueries(query, eventsConnectionsQuery, "INSERT INTO events_connections(mission, mission_time, type, player_id, player_name) VALUES ");
        tryConcatenateQueries(query, eventsGetInOutQuery, "INSERT INTO events_get_in_out(mission, mission_time, type, entity_unit, entity_vehicle) VALUES ");
        tryConcatenateQueries(query, eventsProjectileQuery, "INSERT INTO events_projectile(mission, mission_time, type, entity_attacker, x, y, projectile_name) VALUES ");
        tryConcatenateQueries(query, eventsDownedQuery, "INSERT INTO events_downed(mission, mission_time, type, entity_attacker, entity_victim, attacker_vehicle, same_faction, distance, weapon) VALUES ");
        tryConcatenateQueries(query, eventsMissileQuery, "INSERT INTO events_missile(mission, mission_time, type, entity_attacker, entity_victim, weapon) VALUES ");
        log::trace("Multi statement query: {}", query.str());
        executeMultiStatementQuery(query);
        return hasPoison;
    }

} // namespace sql
} // namespace r3
