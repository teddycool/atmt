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
    MOTOR,
    MOTOR_PWM
};



    extern uint64_t Config_vehicle_ID;
    extern String Config_vehicle_NAME;

    void Config_Begin(void);

    /*String Conig_VehicleName(void);
    motorType_t get_motorType(void);
    steerType_t get_steerType(void);
    boolean get_servoReverse(void);
    int get_steer_servo_min(void);
    int get_steer_servo_max(void);
    int get_steer_servo_adjust(void);
    */

#endif