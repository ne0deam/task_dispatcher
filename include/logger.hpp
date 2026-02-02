#pragma once

#include <cstdio>
#include <memory>
#include <mutex>
#include <string>

class Logger {
public:
    static Logger &Get() {
        static Logger instance;
        return instance;
    }

    void Log(const std::string &message) { fprintf(file_, "%s\n", message.c_str()); }

    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;
    ~Logger() = default;

private:
    Logger() {
        file_ = stdout;
        // отключаем буферизацию
        setvbuf(file_, nullptr, _IONBF, 0);
    }

    FILE *file_;
};
