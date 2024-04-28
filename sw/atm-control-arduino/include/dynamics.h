#include <Adafruit_MPU6050.h>

#ifndef MPU_H
#define MPU_H 

class Dynamics
{
public:
    Dynamics(); // declare default constructor with controlpin input

private:
    Adafruit_MPU6050 mpu;
    float acc_x;
    float acc_y;
    float acc_z;
    float gyro_x;
    float gyro_y;
    float gyro_z;
    float temp;


public:
  void SetUp();
  void Update();
  float GetAccX();
  float GetAccY();
  float GetAccZ();

  float GetGyroX();
  float GetGyroY();
  float GetGyroZ();

};

#endif