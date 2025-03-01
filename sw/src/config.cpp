#include <config.h>

motorType_t motorType;
steerType_t steerType;
boolean servoReverse = false;
int steer_servo_min = 13;
int steer_servo_max = 25;
int steer_servo_adjust = 0;

Config::Config(void){};

void Config::Begin(void)
{
    ID = ESP.getEfuseMac();
    Serial.println("-----------------------");
    Serial.print("This device has id: ");
    Serial.println(ID, HEX);

    switch (ID)
    {
    case 0xE0DE4C08B764: // PAT03
                         // Here we set the global config variables for this truck
        NAME = "PAT03";
        Serial.println("Configures PAT03");
        motorType = SINGLE;
        steerType = SERVO;
        steer_servo_min = 13;
        steer_servo_max = 23;
        steer_servo_adjust = 4;
        break;

    default:
    Serial.println("Unknown vehicle, default config");
    steerType = SERVO;
        break;
    }
};
motorType_t Config::get_motorType(void)
{
    return motorType;
};
steerType_t Config::get_steerType(void)
{
    return steerType;
}

boolean Config::get_servoReverse(void) {
    return servoReverse;
}

int Config::get_steer_servo_min(void) {
    return steer_servo_min;
}

int Config::get_steer_servo_max(void) {
    return steer_servo_max;
}

int Config::get_steer_servo_adjust(void) {
    return steer_servo_adjust;
}