#include <Arduino.h>
#include "setget.h"
#include <Wire.h>
#include <driver/ledc.h>
//#include <Adafruit_VL53L0X.h> //questionable if we should use this as it is not task safe with access to the I2C.....but for a quick test...

#define PWM

// Ultrasound number and pins
#define NUM_SENSORS 4
#define TRIGGER_PIN 16
#define ECHO_PIN 34
#define TRIGGER_PIN2 17
#define ECHO_PIN2 35
#define TRIGGER_PIN3 26
#define ECHO_PIN3 25
#define TRIGGER_PIN4 19
#define ECHO_PIN4 18

//  Laser Lidar ToF __________________________
/*#define VL53L0X_REG_IDENTIFICATION_MODEL_ID 0xc0
#define VL53L0X_REG_IDENTIFICATION_REVISION_ID 0xc2
#define VL53L0X_REG_PRE_RANGE_CONFIG_VCSEL_PERIOD 0x50
#define VL53L0X_REG_FINAL_RANGE_CONFIG_VCSEL_PERIOD 0x70
#define VL53L0X_REG_SYSRANGE_START 0x00

#define VL53L0X_ADDRESS 0x29 */

//------------------------------------

// pins for motor driver L293
// only using one motor as I am building this for servo steering
const int MOTOR_enable_pin = 2;
const int MOTOR_forward_pin = 4;
const int MOTOR_reverse_pin = 5;
const int STEER_enable_pin = 33;
const int STEER_left_pin = 27;
const int STEER_right_pin = 23;

// PWM settings MOTOR
const int freq = 5000;    // Frequency for PWM signal
const int resolution = 8; // Resolution for PWM signal
const int MAX_PWM = pow(2, resolution) - 1;
const int MOTOR_PWM_channel = LEDC_CHANNEL_1; // Use channel 1 for forward direction
const int STEER_PWM_channel = LEDC_CHANNEL_2; // Use channel 2 for backward direction

#define COMPASS_ADDRESS 0x0d // I2C address for the compass
// MPU-6050 I2C address is 0x68
#define MPU6050_ADDR 0x68

// Power management registers for MPU6050
#define PWR_MGMT_1 0x6B
#define PWR_MGMT_2 0x6C

// Setting for the PWM signal to the servo

#define LEDC_BASE_FREQ 50 // LEDC base frequency
#define LEDC_GPIO 32      // The GPIO pin
#define servo_adjust 0
#define servo_neutral 128 + servo_adjust
#define servo_left (servo_neutral - 80)
#define servo_right (servo_neutral + 80)

volatile long distance[NUM_SENSORS];
volatile long startTime[NUM_SENSORS];
volatile int currentSensor = 0;

//Adafruit_VL53L0X lox = Adafruit_VL53L0X();

// Create vecors for the ultrasouns sensor pins
int triggerPins[NUM_SENSORS] = {TRIGGER_PIN, TRIGGER_PIN2, TRIGGER_PIN3, TRIGGER_PIN4};
int echoPins[NUM_SENSORS] = {ECHO_PIN, ECHO_PIN2, ECHO_PIN3, ECHO_PIN4};

void steer_setup()
{
// Setup timer and attach timer to a led pin
#ifdef PWM
  ledcSetup(STEER_PWM_channel, freq, resolution);
  ledcAttachPin(STEER_enable_pin, STEER_PWM_channel);
  ledcWrite(STEER_PWM_channel, 0);
#endif
#ifndef PWM
  pinMode(STEER_enable_pin, OUTPUT);
  pinMode(STEER_left_pin, OUTPUT);
  pinMode(STEER_right_pin, OUTPUT);
#endif
}

void steer_direction(int32_t steer) //+-100%, 0 = straight
{

  if (steer < -100)
    steer = -100;
  if (steer > 100)
    steer = 100;

  // Set duty cycle
  int value = abs(steer * MAX_PWM / 100);
  // Serial.print("Steer:");
  // Serial.println(value);
  if (steer < 0)
  {
    // LEFT
    digitalWrite(STEER_left_pin, HIGH);
    digitalWrite(STEER_right_pin, LOW);
#ifdef PWM
    ledcWrite(STEER_PWM_channel, value);
#endif
#ifndef PWM
    digitalWrite(STEER_enable_pin, HIGH);
#endif
  }
  else if (steer > 0)
  {
    // RIGHT
    digitalWrite(STEER_left_pin, LOW);
    digitalWrite(STEER_right_pin, HIGH);
#ifdef PWM
    ledcWrite(STEER_PWM_channel, value);
#endif
#ifndef PWM
    digitalWrite(STEER_enable_pin, HIGH);
#endif
  }
  else
  {
#ifdef PWM
    ledcWrite(STEER_PWM_channel, 0);
#endif
#ifndef PWM
    digitalWrite(STEER_enable_pin, LOW);
#endif
  }
}

void MOTOR_setup()
{
#ifdef PWM
  ledcSetup(MOTOR_PWM_channel, freq, resolution);
  ledcAttachPin(MOTOR_enable_pin, MOTOR_PWM_channel);
  ledcWrite(MOTOR_PWM_channel, 0);
#endif
#ifndef PWM
  // Set up the motor control pins as outputs
  pinMode(MOTOR_enable_pin, OUTPUT);
  pinMode(MOTOR_forward_pin, OUTPUT);
  pinMode(MOTOR_reverse_pin, OUTPUT);
  digitalWrite(MOTOR_enable_pin, LOW);
#endif
}

void MOTOR_break()
{
  digitalWrite(MOTOR_forward_pin, LOW);
  digitalWrite(MOTOR_reverse_pin, LOW);
#ifdef PWM
  ledcWrite(MOTOR_PWM_channel, MAX_PWM);
#endif
#ifndef PWM
  digitalWrite(MOTOR_enable_pin, HIGH);
#endif
}

//-100 = full speed in reverese, 100 full speed ahead. 0 is stop.
void MOTOR_set_speed(int speed)
{
  if (speed > 100)
  {
    speed = 100;
  }
  if (speed < -100)
  {
    speed = -100;
  }
  int value = abs(speed * MAX_PWM / 100);
  Serial.print("Set speed:");
  Serial.print(speed);
  Serial.print("  = ");
  Serial.println(value);
  if (speed > 0)
  {
    // Forward direction
    digitalWrite(MOTOR_forward_pin, HIGH);
    digitalWrite(MOTOR_reverse_pin, LOW);
#ifdef PWM
    ledcWrite(MOTOR_PWM_channel, value);
#endif
#ifndef PWM
    digitalWrite(MOTOR_enable_pin, HIGH);
#endif
  }
  else if (speed < 0)
  {
    // Backward direction
    digitalWrite(MOTOR_forward_pin, LOW);
    digitalWrite(MOTOR_reverse_pin, HIGH);
#ifdef PWM
    ledcWrite(MOTOR_PWM_channel, value);
#endif
#ifndef PWM
    digitalWrite(MOTOR_enable_pin, HIGH);
#endif
  }
  else
  {
// Stop the motor
#ifdef PWM
    ledcWrite(MOTOR_PWM_channel, 0);
#endif
#ifndef PWM
    digitalWrite(MOTOR_enable_pin, LOW);
#endif
  }
}

void echoInterrupt()
{
  int i = currentSensor;
  if (digitalRead(echoPins[i]) == HIGH)
  {
    // The echo pin went from LOW to HIGH: start timing
    startTime[i] = micros();
  }
  else
  {
    // The echo pin went from HIGH to LOW: stop timing and calculate distance
    long travelTime = micros() - startTime[i];
    switch (i)
    {
    case 0:
      globalVar_set(rawDistFront, travelTime / 29 / 2);
      break;
    case 1:
      globalVar_set(rawDistRight, travelTime / 29 / 2);
      break;
    case 2:
      globalVar_set(rawDistLeft, travelTime / 29 / 2);
      break;
    case 3:
      globalVar_set(rawDistBack, travelTime / 29 / 2);
      break;
    }
    // distance[i] = travelTime / 29 / 2;
  }
}

void SERVICE_pollSensors(void *pvParameters)
{
  for (;;)
  {
    for (int i = 0; i < NUM_SENSORS; i++)
    {
      currentSensor = i;

      // Initialize trigger and echo pins
      pinMode(triggerPins[i], OUTPUT);
      pinMode(echoPins[i], INPUT);

      // Attach an interrupt to the echo pin
      attachInterrupt(digitalPinToInterrupt(echoPins[i]), echoInterrupt, CHANGE);

      // Send a 10 microsecond pulse to start the sensor
      digitalWrite(triggerPins[i], HIGH);
      delayMicroseconds(10);
      digitalWrite(triggerPins[i], LOW);

      // Wait for 100 ms before polling the next sensor
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }
}

void SERVICE_readCompass(void *pvParameters)
{
  for (;;)
  {
    int x, y, z; // variables to store the compass data
    Wire.beginTransmission(COMPASS_ADDRESS);
    Wire.write(0x03); // Request the data starting at register 0x03
    Wire.endTransmission();
    Wire.requestFrom((uint8_t)COMPASS_ADDRESS, 6, true);
    if (6 <= Wire.available())
    {
      x = Wire.read() << 8 | Wire.read();
      z = Wire.read() << 8 | Wire.read();
      y = Wire.read() << 8 | Wire.read();
      globalVar_set(rawMagX, x);
      globalVar_set(rawMagZ, z);
      globalVar_set(rawMagY, y);
      // float heading = atan2(y, x);
      // if (heading < 0)
      /*{
        heading += 2 * PI;
      }
      Serial.print("Heading: ");
      Serial.println(heading * 180 / PI);
      */
    }
    vTaskDelay(pdMS_TO_TICKS(700));
  }
}

void SERVICE_readAccelerometer(void *pvParameters)
{
  for (;;)
  {
    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(0x3B); // starting with register 0x3B (ACCEL_XOUT_H)
    Wire.endTransmission(false);
    Wire.requestFrom((int)MPU6050_ADDR, 14, true); // request a total of 14 registers

    // read accelerometer and gyroscope data
    //+++++++++++++++++++++++++++++++++
    //X direction = forward/backwards of the truck
    int16_t AcX = Wire.read() << 8 | Wire.read(); // 0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)
    //calculate distance
    long distance_age;
    int old_distance;
    int old_speed;
    long speed_age;
    old_speed = globalVar_get(calcSpeed,&speed_age);
    //globalVar_get(calcDistance,old_speed,&speed_age);
    globalVar_set(calcDistance,old_distance + (old_speed*distance_age));  //Distance based on old speed, times how long since last update
    //calculate speed
    globalVar_set(calcSpeed, old_speed + (AcX * speed_age));  //Speed based on old acc, times how long sonce last update
    //finally update the new accelectaion
    globalVar_set(rawAccX, AcX);
    //----------------------------
    int16_t AcY = Wire.read() << 8 | Wire.read(); // 0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
    globalVar_set(rawAccY, AcY);
    int16_t AcZ = Wire.read() << 8 | Wire.read(); // 0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L)
    globalVar_set(rawAccZ, AcZ);
    int16_t Tmp = Wire.read() << 8 | Wire.read(); // 0x41 (TEMP_OUT_H) & 0x42 (TEMP_OUT_L)
    globalVar_set(rawTemp, Tmp / 34 + 365);
    int16_t GyX = Wire.read() << 8 | Wire.read(); // 0x43 (GYRO_XOUT_H) & 0x44 (GYRO_XOUT_L)
    globalVar_set(rawGyX, GyX / 13);
    int16_t GyY = Wire.read() << 8 | Wire.read(); // 0x45 (GYRO_YOUT_H) & 0x46 (GYRO_YOUT_L)
    globalVar_set(rawGyY, GyY / 13);
    //-----------------------
    //Z dimension of the gyro gives us how quickly the direction of the truck changes
    int16_t GyZ = Wire.read() << 8 | Wire.read(); // 0x47 (GYRO_ZOUT_H) & 0x48 (GYRO_ZOUT_L)
    //calculate heading
    long heading_age;
    int old_heading;
    old_heading = globalVar_get (calcHeading,&heading_age);
    globalVar_set(calcHeading,old_heading + (GyZ*heading_age));  //the longer the time since last updtae the more the heading has changed.
    globalVar_set(rawGyZ, GyZ);    //store the latest turn speed
    //------------
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void SERVICE_Lidar_init()
{

  // Check the device ID to confirm that we're talking to a VL53L0X
  /*Wire.beginTransmission(VL53L0X_ADDRESS);
  Wire.write(VL53L0X_REG_IDENTIFICATION_MODEL_ID);
  Wire.endTransmission();
  Wire.requestFrom(VL53L0X_ADDRESS, 1);
  if (Wire.read() != 0xEE)
  {
    Serial.println("Failed to find VL53L0X");
    while (1)
      ;
  }

  // Start ranging
  Wire.beginTransmission(VL53L0X_ADDRESS);
  Wire.write(VL53L0X_REG_SYSRANGE_START);
  Wire.write(0x01);
  Wire.endTransmission();*/
}

void SERVICE_Lidar(void *pvParameters)
{

  uint16_t distance;

  // Read the result
  for (;;)
  {
    /*
    Wire.beginTransmission(VL53L0X_ADDRESS);
    Wire.write(0x14 + 10); // Result register address
    Wire.endTransmission();
    Wire.requestFrom(VL53L0X_ADDRESS, 2);
    distance = Wire.read() << 8;
    distance |= Wire.read();
    globalVar_set(rawLidarFront, distance);
    */

    //VL53L0X_RangingMeasurementData_t measure;

    // Read the distance from the sensor
    //lox.rangingTest(&measure, false); // pass in 'true' to get debug data printout!

    /*if (measure.RangeStatus != 4)
    { // phase failures have incorrect data
      // Serial.print("Distance (mm): ");
      // Serial.println(measure.RangeMilliMeter);
      globalVar_set(rawLidarFront, measure.RangeMilliMeter);
    }
    else
    {
      Serial.println(" out of range ");
    }*/
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void setup()
{
  // Initialize serial communication for debugging
  Serial.begin(57600);
  Wire.begin();
  Wire.setClock(100000);
  Wire.beginTransmission(COMPASS_ADDRESS);
  Wire.write(0x02);
  Wire.write(0x00);
  Wire.endTransmission();
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(PWR_MGMT_1);
  Wire.write(0);
  Wire.endTransmission(true);

  steer_setup();
  MOTOR_setup();
  MOTOR_set_speed(0);

  globalVar_init();
  // Initialize trigger and echo pins
  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // SERVICE_Lidar_init();
  /*if (!lox.begin())
  {
    Serial.println(F("Failed to boot VL53L0X"));
    while (1)
      ;
  }*/

  // Attach an interrupt to the echo pin
  attachInterrupt(digitalPinToInterrupt(ECHO_PIN), echoInterrupt, CHANGE);

  // Create a task for polling the sensors
  xTaskCreate(
      SERVICE_pollSensors, // Task function
      "PollSensors",       // Task name
      2000,                // Stack size (in words, not bytes)
      NULL,                // Task input parameter
      2,                   // Task priority
      NULL                 // Task handle
  );

  /*xTaskCreate(
      SERVICE_readCompass, // Task function
      "readcompass",       // Task name
      2000,                // Stack size (in words, not bytes)
      NULL,                // Task input parameter
      2,                   // Task priority
      NULL                 // Task handle
  ); */

  xTaskCreate(
      SERVICE_readAccelerometer, // Task function
      "readaccelerometer",       // Task name
      2000,                      // Stack size (in words, not bytes)
      NULL,                      // Task input parameter
      2,                         // Task priority
      NULL                       // Task handle
  );

  xTaskCreate(
      SERVICE_Lidar, // Task function
      "lidar",       // Task name
      5000,          // Stack size (in words, not bytes)
      NULL,          // Task input parameter
      2,             // Task priority
      NULL           // Task handle
  );

  // To avoid having 0 front distance
  vTaskDelay(pdMS_TO_TICKS(500));

  // Here we should add a debug print of all sensor values before we start running to make sure everything is working.
  //
  Serial.print("Number of VARs: ");
  Serial.println(NUM_VARS);
  Serial.print("Left: ");
  Serial.print(globalVar_get(rawDistLeft));
  Serial.print(" cm");
  Serial.print("      Right: ");
  Serial.print(globalVar_get(rawDistRight));
  Serial.print(" cm");
  Serial.print("    Front: ");
  Serial.print(globalVar_get(rawDistFront));
  Serial.print(" cm");
  Serial.print("      Back: ");
  Serial.print(globalVar_get(rawDistBack));
  Serial.println(" cm");
  //----------------------------------------------------------------------------------------------------------------
}

void loop()
{
  // Empty. All the work is done in tasks.
  // Print the distance for debugging
  /*
  Serial.print("Number of VARs: ");
  Serial.println(NUM_VARS);
  Serial.print("Left: ");
  Serial.print(globalVar_get(rawDistLeft));
  Serial.print(" cm");
  Serial.print("      Right: ");
  Serial.print(globalVar_get(rawDistRight));
  Serial.print(" cm");
  */

  Serial.print("    Front: ");
  Serial.print(globalVar_get(rawDistFront));
  Serial.print(" cm");

  /*
  Serial.print("      Back: ");
  Serial.print(globalVar_get(rawDistBack));
  Serial.println(" cm");
  Serial.print("X:");
  Serial.print(globalVar_get(rawMagX));
  Serial.print("   Y:");
  Serial.print(globalVar_get(rawMagY));
  Serial.print("   Z:");
  Serial.println(globalVar_get(rawMagZ));
  Serial.print("AX:");
  Serial.print(globalVar_get(rawAccX));
  Serial.print("   AY:");
  Serial.print(globalVar_get(rawAccY));
  Serial.print("   AZ:");
  Serial.println(globalVar_get(rawAccZ));
  Serial.print("Temp:");
  Serial.println(globalVar_get(rawTemp));*/
  Serial.print("GX:");
  Serial.print(globalVar_get(rawGyX) / 131); // degrees per second turn speed
  Serial.print("   GY:");
  Serial.print(globalVar_get(rawGyY) / 131);
  Serial.print("   GZ:");
  Serial.println(globalVar_get(rawGyZ) / 131); // this is the turning axle.
  Serial.println();

  Serial.print("Lidar: ");
  Serial.println(globalVar_get(rawLidarFront));

  // steer_direction(100);

  //====Check if there is any obstacle in front====//
  if (globalVar_get(rawDistFront) < 40 || (globalVar_get(rawDistRight) < 10 && globalVar_get_delta(rawDistRight) < -2) || (globalVar_get(rawDistLeft) < 10 && globalVar_get_delta(rawDistLeft) < -2))
  {
    MOTOR_set_speed(0);
    MOTOR_break();

    vTaskDelay(pdMS_TO_TICKS(500));
    if (globalVar_get(rawDistLeft) < globalVar_get(rawDistRight))
    {
      // if close to left wall, turn wheels left and back off.
      steer_direction(-90);
    }
    else
    {
      steer_direction(90);
    }

    MOTOR_set_speed(-100);
    for (int i = 0; i < 40; i++)
    {
      vTaskDelay(pdMS_TO_TICKS(50));
      if (globalVar_get(rawDistBack) < 30)
        break;
    }
    MOTOR_break();
    // If you want to change main direction, you need to reset the gyro directon after backing up
    // globalVar_reset_total(rawGyZ);
    steer_direction(0);
    vTaskDelay(pdMS_TO_TICKS(500));
  }
  if (globalVar_get_delta(rawDistFront) < -20)
  {
    // We are going to fast, freeroll for a tick.
    MOTOR_set_speed(0);
  }
  else
  {
    MOTOR_set_speed(100);
  }

  // keep away from walls
  if (globalVar_get(rawDistLeft) < 30)
  {
    steer_direction(-100);
    vTaskDelay(pdMS_TO_TICKS(500));
  }
  else if (globalVar_get(rawDistRight) < 30)
  {
    steer_direction(100);
    vTaskDelay(pdMS_TO_TICKS(500));
  }
  vTaskDelay(pdMS_TO_TICKS(25));

  int tmp = globalVar_get_total(rawGyZ);
  if (tmp > 18) // it tended to go left, thus a higher number to turn left
  {
    steer_direction(-100);
  }
  else if (tmp < -20)
  {
    steer_direction(100);
  }
  else
  {
    steer_direction(0);
  }

  /*
   steer_direction(-100);
   vTaskDelay(pdMS_TO_TICKS(500));
   steer_direction(100);
   vTaskDelay(pdMS_TO_TICKS(500));
   */
};
