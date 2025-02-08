#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <iostream>
#include <HTTPClient.h>
#include "secrets.h"

class Logger {
public:
    enum LogLevel {
        INFO,
        WARNING,
        ERROR
    };

    Logger();
    ~Logger();

    void log(const std::string& message, LogLevel level = INFO);

private:
    std::string getTimestamp();
    std::string levelToString(LogLevel level);
};

#endif // LOGGER_H