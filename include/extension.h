#ifndef EXTENSION_H
#define EXTENSION_H

#include "sql.h"
#include "Queue/Queue.h"


#define R3_EXTENSION_VERSION       "2.0.0"

namespace r3 {

    const std::string REQUEST_COMMAND_POISON = "poison";

    const int RESPONSE_RETURN_CODE_ERROR = -1;
    const int RESPONSE_RETURN_CODE_OK = 0;

    const std::string RESPONSE_TYPE_ERROR = "error";
    const std::string RESPONSE_TYPE_OK = "ok";

    const std::string EMPTY_SQF_DATA = "\"\"";

    struct Request {
        std::string command;
        std::vector<std::string> params;
    };

    struct Response {
        std::string type;
        std::string data;
    };

namespace extension {

    bool initialize();
    void finalize();
    int call(char *output, int outputSize, const char *function, const char **args, int argCount);
    Request popRequest();

} // namespace extension
} // namespace r3

#endif // EXTENSION_H