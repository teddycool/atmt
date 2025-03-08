#include <sensors/accsensor.h>
#include <Arduino.h>
#include <Wire.h>
#include <variables/setget.h>
#include <freertos/FreeRTOS.h>
#include <sensors/kalman_filter.h>

#define MPU6050_ADDR 0x68

// Power management registers for MPU6050
#define PWR_MGMT_1 0x6B
#define PWR_MGMT_2 0x6C

//float processNoise, float measurementNoise, float estimationError, float initialValue)

KalmanFilter kalmanFilterX(1.0f, 0.002f, 1.0f, 0);
KalmanFilter kalmanFilterY(1.0f, 0.002f, 1.0f, 0);
KalmanFilter kalmanFilterZ(1.0f, 0.002f, 1.0f, 0);

ACCsensor::ACCsensor()
{
}

static void accel_task(void *pvParameters)
{
    for (;;)
    {
        // Serial.print(".");
        Wire.beginTransmission(MPU6050_ADDR);
        Wire.write(0x3B); // starting with register 0x3B (ACCEL_XOUT_H)
        Wire.endTransmission(false);
        Wire.requestFrom((int)MPU6050_ADDR, 14, true); // request a total of 14 registers

        // read accelerometer and gyroscope data
        //+++++++++++++++++++++++++++++++++
        // X direction = forward/backwards of the truck
        int16_t AcX = Wire.read() << 8 | Wire.read(); // 0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)
        long zAx = globalVar_get(zeroAx);
        // calculate distance
        long distance_age;
        long old_distance;
        long old_speed;
        long speed_age;
        old_speed = globalVar_get(calcSpeed, &speed_age);
        old_distance = globalVar_get(calcDistance, &distance_age);
        // globalVar_get(calcDistance,old_speed,&speed_age);
        globalVar_set(calcDistance, old_distance + (old_speed * distance_age / (8 * 2048))); // Distance based on old speed, times how long since last update, in mm
        // calculate speed
        globalVar_set(calcSpeed, (int)(old_speed + (((AcX - zAx)) * speed_age))); // Speed based on old acc, times how long sonce last update
        // finally update the new accelectaion
        float filteredAcX = kalmanFilterX.updateEstimate((int)AcX - zAx);
        globalVar_set(rawAccX, (int)filteredAcX);
        //----------------------------
        int16_t AcY = Wire.read() << 8 | Wire.read(); // 0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
        float filteredAcY = kalmanFilterY.updateEstimate(AcY);
        globalVar_set(rawAccY, (int)filteredAcY);
        int16_t AcZ = Wire.read() << 8 | Wire.read(); // 0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L)
        float filteredAcZ = kalmanFilterZ.updateEstimate(AcZ);
        globalVar_set(rawAccZ, (int)filteredAcZ);
        int16_t Tmp = Wire.read() << 8 | Wire.read(); // 0x41 (TEMP_OUT_H) & 0x42 (TEMP_OUT_L)
        globalVar_set(rawTemp, Tmp / 34 + 365);
        int16_t GyX = Wire.read() << 8 | Wire.read(); // 0x43 (GYRO_XOUT_H) & 0x44 (GYRO_XOUT_L)
        globalVar_set(rawGyX, GyX / 13);
        int16_t GyY = Wire.read() << 8 | Wire.read(); // 0x45 (GYRO_YOUT_H) & 0x46 (GYRO_YOUT_L)
        globalVar_set(rawGyY, GyY / 13);
        //-----------------------
        // Z dimension of the gyro gives us how quickly the direction of the truck changes
        int16_t GyZ = (Wire.read() << 8 | Wire.read()) / 131 + 2; // 0x47 (GYRO_ZOUT_H) & 0x48 (GYRO_ZOUT_L)
        // calculate heading
        long heading_age;
        long old_heading;
        old_heading = globalVar_get(calcHeading, &heading_age);
        globalVar_set(calcHeading, old_heading + (GyZ * heading_age)); // the longer the time since last updtae the more the heading has changed.
        globalVar_set(rawGyZ, GyZ);                                    // store the latest turn speed
        //------------
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void ACCsensor::Begin()
{

    globalVar_set(calcSpeed, 0);
    globalVar_set(calcDistance, 0);
    globalVar_set(calcHeading, 0);
    Wire.begin();
    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(PWR_MGMT_1);
    Wire.write(0);
    Wire.endTransmission(true);
    // Set the SMPLRT_DIV register for 200 Hz sampling rate (4 = 1000 / (1 + 4) => 200 Hz)
    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(0x19); // SMPLRT_DIV register
    Wire.write(4);    // Set the divider to 4 for 200 Hz sampling rate
    Wire.endTransmission(true);
    // ACCEL_CONFIG register (0x1C) controls the accelerometer range
    // range should be 0 for ±2g, 1 for ±4g, 2 for ±8g, or 3 for ±16g
    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(0x1C);   // Address of ACCEL_CONFIG register
    Wire.write(0 << 3); // Shift the range value to the correct bit position 2 = 8g range
    Wire.endTransmission();

    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(0x75); // WHO_AM_I register
    Wire.endTransmission(false);
    Wire.requestFrom(MPU6050_ADDR, 1); // Request 1 byte (WHO_AM_I)
    if (Wire.available())
    {
        byte whoAmI = Wire.read();
        Serial.print("WHO_AM_I: 0x");
        Serial.println(whoAmI, HEX); // Should print 0x68 if MPU6050 is present
    }
    else
    {
        Serial.println("Error: Failed to read WHO_AM_I register");
    }
    vTaskDelay(pdMS_TO_TICKS(200));
    // We need to store the zero values avergaes
    long n = 0;
    long tmp_AcX;
    for (int i = 1; i < 50; i++)
    {
        Wire.beginTransmission(MPU6050_ADDR);
        Wire.write(0x3B); // starting with register 0x3B (ACCEL_XOUT_H)
        Wire.endTransmission(false);
        Wire.requestFrom((int)MPU6050_ADDR, 14, true); // request a total of 14 registers
        int16_t tmp_AcX = Wire.read() << 8 | Wire.read();
        n += tmp_AcX;
        vTaskDelay(pdMS_TO_TICKS(21));
        globalVar_set(zeroAx, n / i);
    };

    Serial.print("Calculated offset: ");
    Serial.println(globalVar_get(zeroAx));
    // globalVar_set(zeroAx,512);

    Serial.println("Starting ACCsens");
    xTaskCreate(
        accel_task,  // Task function
        "acceltask", // Task name
        2000,        // Stack size (in words, not bytes)
        NULL,        // Task input parameter
        2,           // Task priority
        NULL         // Task handle
    );
}