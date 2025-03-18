#include <Arduino.h>
#include <actuators/steer.h>
#include <config.h>
#include <actuators/motor.h>
#include <variables/setget.h>

// Define the pin for the motor
const int MOTOR_enable_pin = 33;
const int MOTOR_left_pin = 27;
const int MOTOR_right_pin = 23;
// PWM settings steering with MOTOR
const int MOTOR_PWM_freq = 5000; // Frequency for PWM signal
const int MOTOR_resolution = 10; // Resolution for PWM signal
const int MOTOR_MAX_PWM = pow(2, MOTOR_resolution) - 1;
const int MOTOR_PWM_channel = 1;

// Define the pin for the servo
const int SERVO_Pin = 32;           // Change to your GPIO pin
const int SERVO_pwmFreq = 50;       // Frequency for servo control (50Hz)
const int SERVO_pwmResolution = 10; // 8-bit resolution (0-255 range)
const int SERVO_pwmChannel = 2;     // PWM channel, motor control is ch 0 (and ch 1 if differential)
const int SERVO_MAX_PWM = pow(2, SERVO_pwmResolution) - 1;

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
    Serial.println("SERVO-STEERING");
    steer_servo_min = conf2.get_steer_servo_min();
    steer_servo_max = conf2.get_steer_servo_max();
    steer_servo_adjust = conf2.get_steer_servo_adjust();
    // set up light pin for 50Hz operation to control the steering servo
    // Set the servo pin as an output
    pinMode(SERVO_Pin, OUTPUT);
    // Configure the LEDC PWM
    ledcSetup(SERVO_pwmChannel, SERVO_pwmFreq, SERVO_pwmResolution);
    // Attach the pin to the PWM channel
    ledcAttachPin(SERVO_Pin, SERVO_pwmChannel);
    ledcWrite(SERVO_pwmChannel, (steer_servo_max + steer_servo_min) / 2 + steer_servo_adjust); // neutral position
    break;
  }
  case MOTOR:
  {
    Serial.println("MOTOR-STEERING");
    pinMode(MOTOR_left_pin, OUTPUT);
    pinMode(MOTOR_right_pin, OUTPUT);
    pinMode(MOTOR_enable_pin, OUTPUT);

    break;
  }

  case MOTOR_PWM:
  {

    ledcSetup(MOTOR_PWM_channel, MOTOR_PWM_freq, MOTOR_resolution);
    // Attach the pin to the PWM channel
    ledcAttachPin(MOTOR_enable_pin, MOTOR_PWM_channel);
    ledcWrite(MOTOR_PWM_channel, 0); // neutral position
    pinMode(MOTOR_left_pin, OUTPUT);
    pinMode(MOTOR_right_pin, OUTPUT);
    pinMode(MOTOR_enable_pin, OUTPUT);

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
    ledcWrite(SERVO_pwmChannel, dutyCycle);
    break;
  }

  case MOTOR:
  {
    if (direction < 0)
    {
      // LEFT
      digitalWrite(MOTOR_left_pin, HIGH);
      digitalWrite(MOTOR_right_pin, LOW);
      digitalWrite(MOTOR_enable_pin, HIGH);
    }
    else if (direction > 0)
    {
      // RIGHT
      digitalWrite(MOTOR_left_pin, LOW);
      digitalWrite(MOTOR_right_pin, HIGH);
      digitalWrite(MOTOR_enable_pin, HIGH);
    }
    else
    {
      digitalWrite(MOTOR_enable_pin, LOW);
    }
    break;
  }

  case MOTOR_PWM:
  {
    int value = abs(direction * MOTOR_MAX_PWM / 100);
    if (direction < 0)
    {
      // LEFT
      digitalWrite(MOTOR_left_pin, HIGH);
      digitalWrite(MOTOR_right_pin, LOW);
      digitalWrite(MOTOR_enable_pin, HIGH);
    }
    else if (direction > 0)
    {
      // RIGHT
      digitalWrite(MOTOR_left_pin, LOW);
      digitalWrite(MOTOR_right_pin, HIGH);
      digitalWrite(MOTOR_enable_pin, HIGH);
    }
    else
    {
      ledcWrite(MOTOR_PWM_channel, 0);
    }

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
    // Serial.println("Steering right");
    this->direction(100);
    // motor.Reverse();
  }

  void Steer::Left()
  {
    // Serial.println("Steering left");
    this->direction(-100);
    // motor.Start();
  }

  void Steer::Straight()
  {
    // Serial.println("Steering straight");
    this->direction(0);
    // motor.Stop();
  }

  void Steer::Stop() // unlogical name for steering use straight instead
  {
    // Serial.println("Steering stopped");
    this->Straight();
    // motor.Stop();
  }
