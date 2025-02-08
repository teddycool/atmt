#include "logger.h"
#include <ctime>
#include <sstream>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include "secrets.h"

Logger::Logger() {
    // No initialization needed for HTTPClient
}

Logger::~Logger() {
    // No cleanup needed for HTTPClient
}

void Logger::log(const std::string& message, LogLevel level) {
    WiFiClient wificlient;
    HTTPClient http;

    std::string timestamp = getTimestamp();
    std::string logMessage = timestamp + " [" + levelToString(level) + "] " + message;

    std::string encodedMessage = logMessage;
    std::replace(encodedMessage.begin(), encodedMessage.end(), ' ', '%20');
    std::string fullUrl = "http://" + std::string(postserver) + std::string(postresource) + "?msg=" + encodedMessage;

    http.begin(wificlient, fullUrl.c_str());
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
        std::cout << "HTTP Response code: " << httpResponseCode << std::endl;
    } else {
        std::cerr << "Error code: " << httpResponseCode << std::endl;
    }

    http.end();
}

std::string Logger::getTimestamp() {
    // ...existing code...
    std::time_t now = std::time(nullptr);
    std::tm* now_tm = std::localtime(&now);
    std::ostringstream oss;
    oss << (now_tm->tm_year + 1900) << '-'
        << (now_tm->tm_mon + 1) << '-'
        << now_tm->tm_mday << ' '
        << now_tm->tm_hour << ':'
        << now_tm->tm_min << ':'
        << now_tm->tm_sec;
    return oss.str();
}

std::string Logger::levelToString(LogLevel level) {
    // ...existing code...
    switch (level) {
        case INFO: return "INFO";
        case WARNING: return "WARNING";
        case ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}