#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include "dynamics.h"

Dynamics::Dynamics(){    
    }

void Dynamics::SetUp(){
    mpu.begin();
    mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_5_HZ);
}

void Dynamics::Update(){
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    acc_x = a.acceleration.x;
    gyro_x = g.gyro.x;
}


float Dynamics::GetAccX(){
    return acc_x;
}
float Dynamics::GetGyroX(){
    return gyro_x;
}
