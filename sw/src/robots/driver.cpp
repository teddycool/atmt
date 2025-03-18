#include <robots/driver.h>
#include <variables/setget.h>

typedef enum
    {
        NONE,
        DIRECTION,
        TURN
    } driverActivity_t;
    driverActivity_t driverActivity = NONE;
    int desired_direction = 0;
    int desired_turn = 0;
    int desired_distance = 0;
    int desired_speed = 0;

Driver::Driver(){

}

void Driver::Begin(){
  //Start driver task
  
}

void drive_absolute(int direction, int distance, int speed){ // direction is the direction since start in degrees....will drift. Distance measured in mm (is relative)
   desired_direction = direction;
   desired_distance = distance;
   desired_speed = speed;
   driverActivity = DIRECTION;                                                       // will return true if successful. Failure can be caused by the direction was not achieved or distance was not achieved
}

void drive_relative(int turn, int distance, int speed){
    desired_turn = turn;
    desired_distance = distance;
    desired_speed = speed;
    driverActivity = TURN;  
}


static void driverService() {

}