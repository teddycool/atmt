#include <Arduino.h>
#include <actuators/steer.h>
#include <config.h>
#include <actuators/motor.h>
#include <variables/setget.h>

// Define the pin for the servo
const int servoPin = 32;      // Change to your GPIO pin
const int pwmFreq = 50;       // Frequency for servo control (50Hz)
const int pwmResolution = 10; // 8-bit resolution (0-255 range)
const int pwmChannel = 1;     // PWM channel, motor control is ch 0

Config conf2;

Steer::Steer()
{
  Serial.begin(57600);
}

void Steer ::Begin()
{
  globalVar_set(steerDirection, 0);
  steerType = conf2.get_steerType();
  switch (steerType)

  {
  case SERVO:
  {
    Serial.println("SERVO");
    steer_servo_min = conf2.get_steer_servo_min();
    steer_servo_max = conf2.get_steer_servo_max();
    steer_servo_adjust = conf2.get_steer_servo_adjust();
    // set up light pin for 50Hz operation to control the steering servo
    // Set the servo pin as an output
    pinMode(servoPin, OUTPUT);
    // Configure the LEDC PWM
    ledcSetup(pwmChannel, pwmFreq, pwmResolution);
    // Attach the pin to the PWM channel
    ledcAttachPin(servoPin, pwmChannel);
    ledcWrite(pwmChannel, (steer_servo_max + steer_servo_min) / 2 + steer_servo_adjust); // neutral position
    break;
  }
  case MOTOR:
  {
    break;
  }
  default:
  {
    assert(false);
    break;
  }
  }
}

void Steer::direction(int direction)
{
  if (direction > 100)
    direction = 100;
  if (direction < -100)
    direction = -100;

  globalVar_set(steerDirection, direction);

  switch (steerType)
  {
  case SERVO:
  {
    int dutyCycle = map(direction, -100, 100, steer_servo_min, steer_servo_max) + steer_servo_adjust; // Duty cycle range for 1ms to 2ms
    ledcWrite(pwmChannel, dutyCycle);
    break;
  }

  case MOTOR:
  {

    break;
  }
  default:
  {
    assert(false);
    break;
  }
  }
}

//=============================
//= To be depreciated use direction instead
//=============================
void Steer::Right()
{
  Serial.println("Steering right");
  this->direction(100);
  // motor.Reverse();
}

void Steer::Left()
{
  Serial.println("Steering left");
  this->direction(-100);
  // motor.Start();
}

void Steer::Stop()
{
  Serial.println("Steering stopped");
  this->direction(0);
  // motor.Stop();
}
