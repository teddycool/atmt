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
    mag.begin();
}

void Dynamics::Update(){
    sensors_event_t a, g, temp;
    sensors_event_t m;
    mpu.getEvent(&a, &g, &temp);
    mag.getEvent(&m);
    acc_x = a.acceleration.x;
    acc_y = a.acceleration.y;
    acc_z = a.acceleration.z;   

    gyro_x = g.gyro.x;
    gyro_y = g.gyro.y;
    gyro_x = g.gyro.z;

    cmp_x = m.magnetic.x;
    cmp_y = m.magnetic.y;
    cmp_z = m.magnetic.z;

}


float Dynamics::GetAccX(){
    return acc_x;
}
float Dynamics::GetAccY(){
    return acc_y;
}
float Dynamics::GetAccZ(){
    return acc_z;
}

float Dynamics::GetGyroX(){
    return gyro_x;
}
float Dynamics::GetGyroY(){
    return gyro_y;
}
float Dynamics::GetGyroZ(){
    return gyro_z;
}

float Dynamics::GetCompX(){
    return cmp_x;
}
float Dynamics::GetCompY(){
    return cmp_y;
}
float Dynamics::GetCompZ(){
    return cmp_z;
}
