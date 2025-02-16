#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <iostream>
#include <HTTPClient.h>

class Logger {
public:
   
    Logger(String chipid);
    ~Logger();

    void postlog(String msg);    

private:
    String chipid;
};

#endif // LOGGER_H