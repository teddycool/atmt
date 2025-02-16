#include "logger.h"
#include "logger.h"
#include <sstream>
#include <algorithm>
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

    std::string logMessage = "[" + levelToString(level) + "] " + message;

    std::string encodedMessage = logMessage;
 //   std::replace(encodedMessage.begin(), encodedMessage.end(), ' ', '%20');
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

std::string Logger::levelToString(LogLevel level) {
    // ...existing code...
    switch (level) {
        case INFO: return "INFO";
        case WARNING: return "WARNING";
        case ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}