#include "sql.h"

#include "extension.h"
#include "log.h"

#include "Poco/Nullable.h"
#include "Poco/Data/Session.h"
#include "Poco/Data/MySQL/MySQLException.h"
#include "Poco/Data/MySQL/Connector.h"


namespace r3 {
    namespace sql {

        namespace {
            std::string host, database, user, password;
            uint32_t port;
            size_t timeout;
            Poco::Data::Session* session;
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

        Poco::Nullable<double> getNumericValue(const std::vector<std::string>& parameters, const size_t& idx) {
            if (parameters.size() > idx && !parameters[idx].empty()) {
                double number = 0;
                if (!Poco::NumberParser::tryParseFloat(parameters[idx], number)) {
                    return Poco::Nullable<double>();
                }
                return Poco::Nullable<double>(number);
            }
            return Poco::Nullable<double>();
        }

        Poco::Nullable<std::string> getCharValue(const std::vector<std::string>& parameters, const size_t& idx) {
            if (parameters.size() > idx && !parameters[idx].empty()) {
                return Poco::Nullable<std::string>(parameters[idx]);
            }
            return Poco::Nullable<std::string>();
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
            delete session;
            Poco::Data::MySQL::Connector::unregisterConnector();
        }

        void run() {
            auto request = extension::popRequest();
            while (request.command != REQUEST_COMMAND_POISON) {
                {
                    std::lock_guard<std::mutex> lock(sessionMutex);
                    processRequest(request);
                }
                request = extension::popRequest();
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
            try {
                Poco::Data::MySQL::Connector::registerConnector();
                session = new Poco::Data::Session("MySQL", fmt::format("host={};port={};db={};user={};password={};compress=true;auto-reconnect=true", host, port, database, user, password), timeout);
                connected = true;
            }
            catch (Poco::Data::ConnectionFailedException& e) {
                std::string message = fmt::format("Failed to connect to MySQL server! Error code: '{}', Error message: {}", e.code(), e.displayText());
                log::logger->error(message);
                connected = false;
                return message;
            }
            catch (Poco::Data::MySQL::ConnectionException& e) {
                std::string message = fmt::format("Failed to connect to MySQL server! Error code: '{}', Error message: {}", e.code(), e.displayText());
                log::logger->error(message);
                connected = false;
                return message;
            }
            return "";
        }

        Response processRequest(const Request& request) {
            Response response{ RESPONSE_TYPE_OK, EMPTY_SQF_DATA };
            auto paramsSize = request.params.size();
            log::logger->trace("Request command '{}' params size '{}'!", request.command, request.params.size());
            try {
                if (request.command == "create_mission" && paramsSize == 7) {
                    std::string missionName = request.params[0];
                    std::string missionDisplayName = request.params[1];
                    std::string terrain = request.params[2];
                    std::string author = request.params[3];
                    double dayTime = getNumericValue(request.params, 4);
                    std::string addonVersion = request.params[5];
                    std::string fileName = request.params[6];
                    log::logger->debug("Inserting into 'missions' values missionName '{}', missionDisplayName'{}', terrain '{}', author '{}' ,dayTime '{}', addonVersion '{}', fileName '{}'.",
                        missionName, missionDisplayName, terrain, author, dayTime, addonVersion, fileName);
                    *session << "INSERT INTO missions(name, display_name, terrain, author, day_time, created_at, addon_version, file_name) VALUES(?, ?, ?, ?, ?, UTC_TIMESTAMP(), ?, ?)",
                        Poco::Data::Keywords::use(missionName),
                        Poco::Data::Keywords::use(missionDisplayName),
                        Poco::Data::Keywords::use(terrain),
                        Poco::Data::Keywords::use(author),
                        Poco::Data::Keywords::use(dayTime),
                        Poco::Data::Keywords::use(addonVersion),
                        Poco::Data::Keywords::use(fileName),
                        Poco::Data::Keywords::now;
                    uint32_t replayId = 0;
                    *session << "SELECT LAST_INSERT_ID()",
                        Poco::Data::Keywords::into(replayId),
                        Poco::Data::Keywords::now;
                    log::logger->debug("New mission id is '{}'.", replayId);
                    response.data = std::to_string(replayId);
                }
                else if (request.command == "update_mission" && paramsSize == 2) {
                    uint32_t replayId = parseUnsigned(request.params[0]);
                    double missionTime = parseFloat(request.params[1]);
                    log::logger->debug("Updating 'missions' values last_mission_time '{}', id '{}'.", missionTime, replayId);
                    *session << "UPDATE missions SET last_event_time = UTC_TIMESTAMP(), last_mission_time = ? WHERE id = ? LIMIT 1",
                        Poco::Data::Keywords::use(missionTime),
                        Poco::Data::Keywords::use(replayId),
                        Poco::Data::Keywords::now;
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
                    double missionTime = parseFloat(request.params[12]);
                    log::logger->debug("Inserting into 'infantry' values mission '{}', playerId '{}', entityId '{}', name '{}', faction '{}', class '{}', group '{}', leader '{}', icon '{}', weapon '{}', launcher '{}', data '{}', mission_time '{}'.",
                        replayId, playerId, entityId, unitName, unitFaction, unitClass, unitGroupId, unitIsLeader, unitIcon, unitWeapon, unitLauncher, unitData, missionTime);
                    *session << "INSERT INTO infantry(mission, player_id, entity_id, name, faction, class, `group`, leader, icon, weapon, launcher, data, mission_time) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)",
                        Poco::Data::Keywords::use(replayId),
                        Poco::Data::Keywords::use(playerId),
                        Poco::Data::Keywords::use(entityId),
                        Poco::Data::Keywords::use(unitName),
                        Poco::Data::Keywords::use(unitFaction),
                        Poco::Data::Keywords::use(unitClass),
                        Poco::Data::Keywords::use(unitGroupId),
                        Poco::Data::Keywords::use(unitIsLeader),
                        Poco::Data::Keywords::use(unitIcon),
                        Poco::Data::Keywords::use(unitWeapon),
                        Poco::Data::Keywords::use(unitLauncher),
                        Poco::Data::Keywords::use(unitData),
                        Poco::Data::Keywords::use(missionTime),
                        Poco::Data::Keywords::now;
                }
                else if (request.command == "infantry_positions" && paramsSize == 8) {
                    uint32_t replayId = parseUnsigned(request.params[0]);
                    uint32_t entityId = parseUnsigned(request.params[1]);
                    double posX = parseFloat(request.params[2]);
                    double posY = parseFloat(request.params[3]);
                    uint32_t direction = parseUnsigned(request.params[4]);
                    uint32_t keyFrame = parseUnsigned(request.params[5]);
                    uint32_t isDead = parseUnsigned(request.params[6]);
                    double missionTime = parseFloat(request.params[7]);
                    log::logger->debug("Inserting into 'infantry_positions' values mission '{}', entity_id '{}', x '{}', y '{}', direction '{}', key_frame '{}', is_dead '{}', mission_time '{}'.",
                        replayId, entityId, posX, posY, direction, keyFrame, isDead, missionTime);
                    *session << "INSERT INTO infantry_positions(mission, entity_id, x, y, direction, key_frame, is_dead, mission_time) VALUES (?,?,?,?,?,?,?,?)",
                        Poco::Data::Keywords::use(replayId),
                        Poco::Data::Keywords::use(entityId),
                        Poco::Data::Keywords::use(posX),
                        Poco::Data::Keywords::use(posY),
                        Poco::Data::Keywords::use(direction),
                        Poco::Data::Keywords::use(keyFrame),
                        Poco::Data::Keywords::use(isDead),
                        Poco::Data::Keywords::use(missionTime),
                        Poco::Data::Keywords::now;
                }
                else if (request.command == "vehicles" && paramsSize == 6) {
                    uint32_t replayId = parseUnsigned(request.params[0]);
                    uint32_t entityId = parseUnsigned(request.params[1]);
                    std::string vehicleClass = request.params[2];
                    std::string vehicleIcon = request.params[3];
                    std::string vehicleIconPath = request.params[4];
                    double missionTime = parseFloat(request.params[5]);
                    log::logger->debug("Inserting into 'vehicles' values mission '{}', entity_id '{}', class '{}', icon '{}', icon_path '{}', mission_time '{}'.",
                        replayId, entityId, vehicleClass, vehicleIcon, vehicleIconPath, missionTime);
                    *session << "INSERT INTO vehicles(mission, entity_id, class, icon, icon_path, mission_time) VALUES (?,?,?,?,?,?)",
                        Poco::Data::Keywords::use(replayId),
                        Poco::Data::Keywords::use(entityId),
                        Poco::Data::Keywords::use(vehicleClass),
                        Poco::Data::Keywords::use(vehicleIcon),
                        Poco::Data::Keywords::use(vehicleIconPath),
                        Poco::Data::Keywords::use(missionTime),
                        Poco::Data::Keywords::now;
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
                    double missionTime = parseFloat(request.params[11]);
                    log::logger->debug("Inserting into 'vehicle_positions' values mission '{}', entity_id '{}', x '{}', y '{}', z '{}', direction '{}', key_frame '{}', driver '{}', crew '{}', cargo '{}', is_dead '{}', mission_time '{}'.",
                        replayId, entityId, posX, posY, posZ, direction, keyFrame, driver, crew, cargo, isDead, missionTime);
                    *session << "INSERT INTO vehicle_positions(mission, entity_id, x, y, z, direction, key_frame, driver, crew, cargo, is_dead, mission_time) VALUES (?,?,?,?,?,?,?,?,?,?,?,?)",
                        Poco::Data::Keywords::use(replayId),
                        Poco::Data::Keywords::use(entityId),
                        Poco::Data::Keywords::use(posX),
                        Poco::Data::Keywords::use(posY),
                        Poco::Data::Keywords::use(posZ),
                        Poco::Data::Keywords::use(direction),
                        Poco::Data::Keywords::use(keyFrame),
                        Poco::Data::Keywords::use(driver),
                        Poco::Data::Keywords::use(crew),
                        Poco::Data::Keywords::use(cargo),
                        Poco::Data::Keywords::use(isDead),
                        Poco::Data::Keywords::use(missionTime),
                        Poco::Data::Keywords::now;
                }
                else if (request.command == "events_connections" && paramsSize == 5) {
                    uint32_t replayId = parseUnsigned(request.params[0]);
                    double missionTime = parseFloat(request.params[1]);
                    std::string type = request.params[2];
                    std::string playerId = request.params[3];
                    std::string name = request.params[4];
                    log::logger->debug("Inserting into 'events_connections' values mission '{}', mission_time '{}', type '{}', player_id '{}', player_name '{}'.",
                        replayId, missionTime, type, playerId, name);
                    *session << "INSERT INTO events_connections(mission, mission_time, type, player_id, player_name) VALUES (?, ?, ?, ?, ?)",
                        Poco::Data::Keywords::use(replayId),
                        Poco::Data::Keywords::use(missionTime),
                        Poco::Data::Keywords::use(type),
                        Poco::Data::Keywords::use(playerId),
                        Poco::Data::Keywords::use(name),
                        Poco::Data::Keywords::now;
                }
                else if (request.command == "events_get_in_out" && paramsSize == 5) {
                    uint32_t replayId = parseUnsigned(request.params[0]);
                    double missionTime = parseFloat(request.params[1]);
                    std::string type = request.params[2];
                    uint32_t entityUnit = parseUnsigned(request.params[3]);
                    uint32_t entityVehicle = parseUnsigned(request.params[4]);
                    log::logger->debug("Inserting into 'events_get_in_out' values mission '{}', mission_time '{}', type '{}', entity_unit '{}', entity_vehicle '{}'.",
                        replayId, missionTime, type, entityUnit, entityVehicle);
                    *session << "INSERT INTO events_get_in_out(mission, mission_time, type, entity_unit, entity_vehicle) VALUES (?, ?, ?, ?, ?)",
                        Poco::Data::Keywords::use(replayId),
                        Poco::Data::Keywords::use(missionTime),
                        Poco::Data::Keywords::use(type),
                        Poco::Data::Keywords::use(entityUnit),
                        Poco::Data::Keywords::use(entityVehicle),
                        Poco::Data::Keywords::now;
                }
                else if (request.command == "events_projectile" && paramsSize == 7) {
                    uint32_t replayId = parseUnsigned(request.params[0]);
                    double missionTime = parseFloat(request.params[1]);
                    std::string grenadeType = request.params[2];
                    uint32_t entitAttacker = parseUnsigned(request.params[3]);
                    double posX = parseFloat(request.params[4]);
                    double posY = parseFloat(request.params[5]);
                    std::string projectileName = request.params[6];
                    log::logger->debug("Inserting into 'events_projectile' values mission '{}', mission_time '{}', type '{}', entity_attacker '{}', x '{}', y '{}', projectile_name '{}'.",
                        replayId, missionTime, grenadeType, entitAttacker, posX, posY, projectileName);
                    *session << "INSERT INTO events_projectile(mission, mission_time, type, entity_attacker, x, y, projectile_name) VALUES (?, ?, ?, ?, ?, ?, ?)",
                        Poco::Data::Keywords::use(replayId),
                        Poco::Data::Keywords::use(missionTime),
                        Poco::Data::Keywords::use(grenadeType),
                        Poco::Data::Keywords::use(entitAttacker),
                        Poco::Data::Keywords::use(posX),
                        Poco::Data::Keywords::use(posY),
                        Poco::Data::Keywords::use(projectileName),
                        Poco::Data::Keywords::now;
                }
                else if (request.command == "events_downed" && paramsSize == 9) {
                    uint32_t replayId = parseUnsigned(request.params[0]);
                    double missionTime = parseFloat(request.params[1]);
                    std::string type = request.params[2];
                    uint32_t entityAttacker = parseUnsigned(request.params[3]);
                    uint32_t entityVictim = parseUnsigned(request.params[4]);
                    uint32_t attackerVehicle = parseUnsigned(request.params[5]);
                    uint32_t sameFaction = parseUnsigned(request.params[6]);
                    uint32_t attackerDistance = parseUnsigned(request.params[7]);
                    std::string weapon = request.params[8];
                    log::logger->debug("Inserting into 'events_downed' values mission '{}', mission_time '{}', type '{}', entity_attacker '{}', entity_victim '{}', attacker_vehicle '{}', same_faction '{}', distance '{}', weapon '{}'.",
                        replayId, missionTime, type, entityAttacker, entityVictim, attackerVehicle, sameFaction, attackerDistance, weapon);
                    *session << "INSERT INTO events_downed(mission, mission_time, type, entity_attacker, entity_victim, attacker_vehicle, same_faction, distance, weapon) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)",
                        Poco::Data::Keywords::use(replayId),
                        Poco::Data::Keywords::use(missionTime),
                        Poco::Data::Keywords::use(type),
                        Poco::Data::Keywords::use(entityAttacker),
                        Poco::Data::Keywords::use(entityVictim),
                        Poco::Data::Keywords::use(attackerVehicle),
                        Poco::Data::Keywords::use(sameFaction),
                        Poco::Data::Keywords::use(attackerDistance),
                        Poco::Data::Keywords::use(weapon),
                        Poco::Data::Keywords::now;
                }
                else if (request.command == "events_missile" && paramsSize == 6) {
                    uint32_t replayId = parseUnsigned(request.params[0]);
                    double missionTime = parseFloat(request.params[1]);
                    std::string type = request.params[2];
                    uint32_t entitAttacker = parseUnsigned(request.params[3]);
                    uint32_t entityVictim = parseUnsigned(request.params[4]);
                    std::string weapon = request.params[5];
                    log::logger->debug("Inserting into 'events_missile' values mission '{}', mission_time '{}', type '{}', entity_attacker '{}', entity_victim '{}', weapon '{}'.",
                        replayId, missionTime, type, entitAttacker, entityVictim, weapon);
                    *session << "INSERT INTO events_missile(mission, mission_time, type, entity_attacker, entity_victim, weapon) VALUES (?, ?, ?, ?, ?, ?)",
                        Poco::Data::Keywords::use(replayId),
                        Poco::Data::Keywords::use(missionTime),
                        Poco::Data::Keywords::use(type),
                        Poco::Data::Keywords::use(entitAttacker),
                        Poco::Data::Keywords::use(entityVictim),
                        Poco::Data::Keywords::use(weapon),
                        Poco::Data::Keywords::now;
                }
                else {
                    log::logger->debug("Invlaid command type '{}'!", request.command);
                    response.type = RESPONSE_TYPE_ERROR;
                    response.data = fmt::format("\"Invalid command type!\"");
                }
            }
            catch (Poco::Data::MySQL::MySQLException& e) {
                log::logger->error("Error executing prepared statement! Error code: '{}', Error message: {}", e.code(), e.displayText());
                response.type = RESPONSE_TYPE_ERROR;
                response.data = fmt::format("\"Error executing prepared statement! {}\"", e.displayText());
            }
            return response;
        }

    } // namespace sql
} // namespace r3
