#include <Arduino.h>
#include "setget.h"
#include <Wire.h>
#include <driver/ledc.h>

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

//pins for motor driver L293
//only using one motor as I am building this for servo steering
const int enablePin = 2;
const int input1Pin = 4;
const int input2Pin = 5;
// PWM settings
const int freq = 5000; // Frequency for PWM signal
const int resolution = 8; // Resolution for PWM signal
const int channel1 = LEDC_CHANNEL_1; // Use channel 1 for forward direction
const int channel2 = LEDC_CHANNEL_2; // Use channel 2 for backward direction

#define COMPASS_ADDRESS 0x0d // I2C address for the compass
// MPU-6050 I2C address is 0x68
#define MPU6050_ADDR 0x68

// Power management registers for MPU6050
#define PWR_MGMT_1 0x6B
#define PWR_MGMT_2 0x6C

// Setting for the PWM signal to the servo
#define LEDC_CHANNEL_0 0
#define LEDC_TIMER_13_BIT 13
#define LEDC_BASE_FREQ 50 // LEDC base frequency
#define LEDC_GPIO 32      // The GPIO pin
#define servo_adjust +150
#define servo_neutral 1023 / 2 + servo_adjust
#define servo_left servo_neutral - 70
#define servo_right servo_neutral + 70

volatile long distance[NUM_SENSORS];
volatile long startTime[NUM_SENSORS];
volatile int currentSensor = 0;

// Create vecors for the ultrasouns sensor pins
int triggerPins[NUM_SENSORS] = {TRIGGER_PIN, TRIGGER_PIN2, TRIGGER_PIN3, TRIGGER_PIN4};
int echoPins[NUM_SENSORS] = {ECHO_PIN, ECHO_PIN2, ECHO_PIN3, ECHO_PIN4};

void PWM_setup()
{
  // Setup timer and attach timer to a led pin
  ledcSetup(LEDC_CHANNEL_0, LEDC_BASE_FREQ, LEDC_TIMER_13_BIT);
  ledcAttachPin(LEDC_GPIO, LEDC_CHANNEL_0);
  ledcWrite(LEDC_CHANNEL_0, 600);
}



void PWM_setDutyCycle(uint32_t dutyCycle)
{
  // Set duty cycle
  ledcWrite(LEDC_CHANNEL_0, dutyCycle);
}

void MOTOR_setup() {
    // Set up the motor control pins as outputs
  pinMode(enablePin, OUTPUT);
  pinMode(input1Pin, OUTPUT);
  pinMode(input2Pin, OUTPUT);

}

//-100 = full speed in reverese, 100 full speed ahead. 0 is stop.
void MOTOR_set_speed (int speed) {


if (speed > 0) {
    // Forward direction
    digitalWrite(enablePin, HIGH);
    digitalWrite(input1Pin, HIGH);
    digitalWrite(input2Pin, LOW);
  } else if (speed < 0) {
    // Backward direction
    digitalWrite(enablePin, HIGH);
       digitalWrite(input1Pin, LOW);
    digitalWrite(input2Pin, HIGH);
  } else {
    // Stop the motor
    digitalWrite(enablePin, LOW);
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
    Wire.requestFrom((int)COMPASS_ADDRESS, 6, true);
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
    int16_t AcX = Wire.read() << 8 | Wire.read(); // 0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)
    globalVar_set(rawAccX, AcX);
    int16_t AcY = Wire.read() << 8 | Wire.read(); // 0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
    globalVar_set(rawAccY, AcY);
    int16_t AcZ = Wire.read() << 8 | Wire.read(); // 0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L)
    globalVar_set(rawAccZ, AcZ);
    int16_t Tmp = Wire.read() << 8 | Wire.read(); // 0x41 (TEMP_OUT_H) & 0x42 (TEMP_OUT_L)
    globalVar_set(rawTemp, Tmp / 34 + 365);
    int16_t GyX = Wire.read() << 8 | Wire.read(); // 0x43 (GYRO_XOUT_H) & 0x44 (GYRO_XOUT_L)
    globalVar_set(rawGyX, GyX);
    int16_t GyY = Wire.read() << 8 | Wire.read(); // 0x45 (GYRO_YOUT_H) & 0x46 (GYRO_YOUT_L)
    globalVar_set(rawGyY, GyY);
    int16_t GyZ = Wire.read() << 8 | Wire.read(); // 0x47 (GYRO_ZOUT_H) & 0x48 (GYRO_ZOUT_L)
    globalVar_set(rawGyZ, GyZ);
    vTaskDelay(pdMS_TO_TICKS(50));
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

  PWM_setup();
  MOTOR_setup();
  MOTOR_set_speed(0);

  globalVar_init();
  // Initialize trigger and echo pins
  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

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

  xTaskCreate(
      SERVICE_readCompass, // Task function
      "readcompass",       // Task name
      2000,                // Stack size (in words, not bytes)
      NULL,                // Task input parameter
      2,                   // Task priority
      NULL                 // Task handle
  );

  xTaskCreate(
      SERVICE_readAccelerometer, // Task function
      "readaccelerometer",       // Task name
      2000,                      // Stack size (in words, not bytes)
      NULL,                      // Task input parameter
      2,                         // Task priority
      NULL                       // Task handle
  );
}

void loop()
{
  // Empty. All the work is done in tasks.
  // Print the distance for debugging
  Serial.print("Number of VARs: ");
  Serial.println(NUM_VARS);
  Serial.print("Left: ");
  // Serial.print(distance[currentSensor]);
  Serial.print(globalVar_get(rawDistLeft));
  Serial.print(" cm");
  Serial.print("      Right: ");
  // Serial.print(distance[currentSensor]);
  Serial.print(globalVar_get(rawDistRight));
  Serial.print(" cm");
    Serial.print("    Front: ");
  // Serial.print(distance[currentSensor]);
  Serial.print(globalVar_get(rawDistFront));
  Serial.print(" cm");
  Serial.print("      Back: ");
  // Serial.print(distance[currentSensor]);
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
  Serial.println(globalVar_get(rawTemp));
  Serial.print("GX:");
  Serial.print(globalVar_get(rawGyX));
  Serial.print("   GY:");
  Serial.print(globalVar_get(rawGyY));
  Serial.print("   GZ:");
  Serial.println(globalVar_get(rawGyZ));
  Serial.println();
  PWM_setDutyCycle(random(servo_left, servo_right));
  MOTOR_set_speed(random(-100,100));
  vTaskDelay(pdMS_TO_TICKS(2100));
}
