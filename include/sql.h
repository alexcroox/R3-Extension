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

    bool initialize(const std::string& host_, uint32_t port_, const std::string& database_, const std::string& user_, const std::string& password_, size_t timeout_);
    void finalize();
    void run();
    std::mutex& getSessionMutex();
    bool isConnected();
    std::string connect();
    Response processCreateMissionRequest(const Request& request);
    bool processRequests(const std::vector<Request>& requests);

} // namespace sql
} // namespace r3

#endif // SQL_H
