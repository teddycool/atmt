// this packet allows for individual configs of the trucks
#include <Arduino.h>

#ifndef CONFIG_H
#define CONFIG_H

class Config
{
public:
    Config();

public:
    // drive_type_t DRIVE_TYPE;
    // steer_type_t STEER_TYPE;
    uint64_t ID = 0;
    String NAME;
};

#endif