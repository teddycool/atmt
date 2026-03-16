#include <config.h>

motorType_t motorType;
steerType_t steerType;
boolean servoReverse = false;
int steer_servo_min = 51;
int steer_servo_max = 102;
int steer_servo_adjust = 0;

Config::Config(void) {};

void Config::Begin(void)
{
    Serial.println("-----------------------");
    Serial.println("Configuring truck settings");
    
    // Standard truck configuration
    motorType = SINGLE;
    steerType = MOTOR;
    steer_servo_min = 60;
    steer_servo_max = 105;
    steer_servo_adjust = 5;
    
    Serial.println("Configuration completed");
};
motorType_t Config::get_motorType(void)
{
    return motorType;
};
steerType_t Config::get_steerType(void)
{
    return steerType;
}

boolean Config::get_servoReverse(void)
{
    return servoReverse;
}

int Config::get_steer_servo_min(void)
{
    return steer_servo_min;
}

int Config::get_steer_servo_max(void)
{
    return steer_servo_max;
}

int Config::get_steer_servo_adjust(void)
{
    return steer_servo_adjust;
}