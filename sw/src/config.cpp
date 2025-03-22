#include <config.h>
#include <variables/setget.h>

motorType_t motorType;
steerType_t steerType;
boolean servoReverse = false;
int steer_servo_min = 51;
int steer_servo_max = 102;
int steer_servo_adjust = 0;

bool Config_initiated = false;
uint64_t Config_vehicle_ID = 0;
String Config_vehicle_NAME;

void Config_Begin(void)

{
    // It would not be a problem if this call was repeated, but best to use the same design here.
    if (!Config_initiated)
    {
        Setget_Begin();
        // Default values
        Setget_Set(vehicle_motorType, SINGLE);
        Setget_Set(vehicle_steerType, MOTOR);
        Setget_Set(vehicle_steerServoMin, 51);
        Setget_Set(vehicle_steerServoMax, 102);
        Setget_Set(vehicle_steerServoOffset, 0);
        Setget_Set(vehicle_steerServoRerverse, false);
        Config_vehicle_ID = ESP.getEfuseMac();

        switch (Config_vehicle_ID)
        {
        case 0xE0DE4C08B764: // PAT03 (4x2)
                             // Here we set the global config variables for this truck
            Config_vehicle_NAME = "PAT03";
            Setget_Set(vehicle_motorType, SINGLE);
            Setget_Set(vehicle_steerType, SERVO);
            Setget_Set(vehicle_steerServoMin, 60);
            Setget_Set(vehicle_steerServoMax, 105);
            Setget_Set(vehicle_steerServoOffset, 5);
            Setget_Set(vehicle_steerServoRerverse, false);
            break;

        case 0xCC328A0A8AB4: // PAT04  (6x4)
            Config_vehicle_NAME = "PAT04";
            Setget_Set(vehicle_motorType, DIFFERENTIAL);
            Setget_Set(vehicle_steerType, SERVO);
            Setget_Set(vehicle_steerServoMin, 60);
            Setget_Set(vehicle_steerServoMax, 105);
            Setget_Set(vehicle_steerServoOffset, 5);
            Setget_Set(vehicle_steerServoRerverse, false);
            break;

        case 0xB4328A0A8AB4: // PÄR01
            // Here we set the global config variables for this truck
            Config_vehicle_NAME = "PÄR01";
            Setget_Set(vehicle_motorType, SINGLE);
            Setget_Set(vehicle_steerType, MOTOR);
            break;

        case 0xFC318A0A8AB4: // PÄR02
            // Here we set the global config variables for this truck
            Config_vehicle_NAME = "PÄR02";
            Setget_Set(vehicle_motorType, SINGLE);
            Setget_Set(vehicle_steerType, MOTOR);
            break;

        default:
            Config_vehicle_NAME = "NN";
            Setget_Set(vehicle_motorType, SINGLE);
            Setget_Set(vehicle_steerType, MOTOR);
            break;
        }
        Config_initiated = true;
    }
};