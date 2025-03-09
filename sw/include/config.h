// this packet allows for individual configs of the trucks
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>



enum motorType_t
{
    SINGLE,
    DIFFERENTIAL
};

enum steerType_t
{
    SERVO,
    MOTOR
};

class Config
{
private:
    uint64_t ID = 0;
    String NAME;

public:
    Config(void);
    void Begin(void);

    motorType_t get_motorType(void);
    steerType_t get_steerType(void);
    boolean get_servoReverse(void);
    int get_steer_servo_min(void);
    int get_steer_servo_max(void);
    int get_steer_servo_adjust(void);
};

#endif