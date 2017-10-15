#ifndef SQL_H
#define SQL_H

#include <string>
#include <mutex>
#include <vector>

#define MAX_PROCESS_REQUEST_COUNT 256

namespace r3 {

    struct Request;
    struct Response;

namespace sql {

    void finalize();
    void run();
    std::mutex& getSessionMutex();
    bool isConnected();
    std::string connect(const std::string& host, uint32_t port, const std::string& database, const std::string& user, const std::string& password);
    Response processCreateMissionRequest(const Request& request);
    bool processRequests(const std::vector<Request>& requests);

} // namespace sql
} // namespace r3

#endif // SQL_H
