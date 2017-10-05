#include "extension.h"

//#define R3_CONSOLE
#ifndef R3_CONSOLE

// Windows
#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

BOOL APIENTRY DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        r3::extension::initialize();
        break;

    case DLL_PROCESS_DETACH:
        r3::extension::finalize();
        break;
    }
    return true;
}

extern "C" {
    __declspec(dllexport) void __stdcall RVExtensionVersion(char *output, int outputSize);
    __declspec(dllexport) void __stdcall RVExtension(char *output, int outputSize, const char *function);
    __declspec(dllexport) int __stdcall RVExtensionArgs(char *output, int outputSize, const char *function, const char **args, int argCnt);
};

void __stdcall RVExtensionVersion(char *output, int outputSize) {
    strncpy_s(output, outputSize, R3_EXTENSION_VERSION, _TRUNCATE);
}

void __stdcall RVExtension(char *output, int outputSize, const char *function) {
    outputSize -= 1;
    r3::extension::call(output, outputSize, function, nullptr, 0);
}

int __stdcall RVExtensionArgs(char *output, int outputSize, const char *function, const char **args, int argCnt) {
    outputSize -= 1;
    return r3::extension::call(output, outputSize, function, args, argCnt);
}

// Linux with GCC
#else

extern "C" {
    void RVExtensionVersion(char *output, int outputSize);
    void RVExtension(char *output, int outputSize, const char *function);
    int RVExtensionArgs(char *output, int outputSize, const char *function, const char **args, int argCnt);
}

void RVExtension(char *output, int outputSize, const char *function) {
    outputSize -= 1;
    r3::extension::call(output, outputSize, function, nullptr, 0);
}

int RVExtensionArgs(char *output, int outputSize, const char *function, const char **args, int argCnt) {
    outputSize -= 1;
    return r3::extension::call(output, outputSize, function, args, argCnt);
}

__attribute__((constructor))
static void extension_init() {
    r3::extension::initialize();
}

__attribute__((destructor))
static void extension_finalize() {
    r3::extension::finalize();
}

#endif

#else

#include <iostream>
#include <string>
#include <regex>

int main(int argc, char* argv[]) {
    std::string line = "";
    const int outputSize = 10000;
    char *output = new char[outputSize];
    std::regex commandSeparatorRegex("~");
    std::regex paramSeparatorRegex("`");

    r3::extension::initialize();
    std::cout
        << "Type 'exit' to close console." << std::endl
        << "Use it as <command>~<param1>`<param2>`<param3>... (No support for escaping '~' and '`')" << std::endl
        << "You first have to connect to the DB with 'connect'" << std::endl
        << std::endl << std::endl;
    while (true) {
        std::getline(std::cin, line);
        if (line == "exit") { break; }
        std::vector<std::string> commandAndParams;
        commandAndParams = { std::sregex_token_iterator(line.begin(), line.end(), commandSeparatorRegex, -1), std::sregex_token_iterator() };
        std::string command = commandAndParams[0];
        std::vector<std::string> params;
        if (commandAndParams.size() >= 2) {
            params = { std::sregex_token_iterator(commandAndParams[1].begin(), commandAndParams[1].end(), paramSeparatorRegex, -1), std::sregex_token_iterator() };
        }

        char** args = new char*[params.size()];
        for (size_t i = 0; i < params.size(); i++) {
            args[i] = new char[params[i].size() + 1];
            strcpy(args[i], params[i].c_str());
        }
        const char** constArgs = const_cast<const char**>(args);

        int returnCode = r3::extension::call(output, outputSize, command.c_str(), constArgs, params.size());
        std::cout << "R3: " << "[" << returnCode << "] " << output << std::endl;

        for (size_t i = 0; i < params.size(); i++) {
            delete[] args[i];
        }
        delete[] args;
    }
    r3::extension::finalize();
    return 0;
}

#endif