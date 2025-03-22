#include <Arduino.h>
#include <actuators/motor.h>
#include <config.h>
#include <variables/setget.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Motor 1 (left or both sides if SINGLE motor)
#define ENABLE_ch1_PIN 2
#define FORWARD_ch1_PIN 4
#define REVERSE_ch1_PIN 5

// Motor 2 (right, when DIFFERENTIAL motor)
#define ENABLE_ch2_PIN 33
#define FORWARD_ch2_PIN 27
#define REVERSE_ch2_PIN 23

#define LEFT_MOTOR_PWM_CHANNEL 0
#define RIGHT_MOTOR_PWM_CHANNEL 1
#define MOTOR_PWM_FREQUENCY 5000 // this can be increased later, but hainv an audiable frquency helps the debug phase
                                 // the L293 might be able to handle all the way up to 20000, but 10-12000 should be enough

static SemaphoreHandle_t Motor_Mutex = NULL;

// left or single
uint8_t enable_ch1_pin = 0;
uint8_t forward_ch1_pin = 0;
uint8_t reverse_ch1_pin = 0;

// right when differential
uint8_t enable_ch2_pin = 0;
uint8_t forward_ch2_pin = 0;
uint8_t reverse_ch2_pin = 0;

motorType_t  Motor_motorType;

bool Motor_initialised = false;

void Motor_Begin(void)
{
  if (!Motor_initialised)
  {
    Config_Begin();
    Motor_Mutex = xSemaphoreCreateMutex();
    Motor_motorType = (motorType_t)Setget_Get(vehicle_motorType);
    switch (Motor_motorType)
    {
    case SINGLE:
      // We are using a single motor for propulsion. We will use the first controller channel for that
      //  the first channel will be the LEFT channel when using differential as below
      enable_ch1_pin = ENABLE_ch1_PIN;
      forward_ch1_pin = FORWARD_ch1_PIN;
      reverse_ch1_pin = REVERSE_ch1_PIN;
      // set-up PWM on ENABLE channel, and start with 0 duty cycle

      // set up pins and start with breaks on
      digitalWrite(forward_ch1_pin, LOW);
      digitalWrite(reverse_ch1_pin, LOW);
      pinMode(forward_ch1_pin, OUTPUT);
      pinMode(reverse_ch1_pin, OUTPUT);
      // Initialize motor PWM (5 kHz)
      ledcSetup(LEFT_MOTOR_PWM_CHANNEL, MOTOR_PWM_FREQUENCY, 8); // Channel 0, 5 kHz frequency, 8-bit resolution
      ledcAttachPin(enable_ch1_pin, LEFT_MOTOR_PWM_CHANNEL);     // Attach motorPin to PWM channel 0
      ledcWrite(LEFT_MOTOR_PWM_CHANNEL, 0);
      break;

    case DIFFERENTIAL:
      // we are using separat motors for left and right side, providing more power
      // acting as an electrical differential, and allowing for steering enhancement
      // by also using different throttle on left and right side, both for drive and brake.
      enable_ch1_pin = ENABLE_ch1_PIN;
      forward_ch1_pin = FORWARD_ch1_PIN;
      reverse_ch1_pin = REVERSE_ch1_PIN;
      enable_ch2_pin = ENABLE_ch2_PIN;
      forward_ch2_pin = FORWARD_ch2_PIN;
      reverse_ch2_pin = REVERSE_ch2_PIN;
      // set-up PWM on ENABLE channel, and start with 0 duty cycle

      // set up pins and start with breaks on
      digitalWrite(forward_ch1_pin, LOW);
      digitalWrite(reverse_ch1_pin, LOW);
      pinMode(forward_ch1_pin, OUTPUT);
      pinMode(reverse_ch1_pin, OUTPUT);
      // Initialize motor PWM (5 kHz)
      ledcSetup(LEFT_MOTOR_PWM_CHANNEL, MOTOR_PWM_FREQUENCY, 8); // Channel 0, 5 kHz frequency, 8-bit resolution
      ledcAttachPin(enable_ch1_pin, LEFT_MOTOR_PWM_CHANNEL);     // Attach motorPin to PWM channel 0
      ledcWrite(LEFT_MOTOR_PWM_CHANNEL, 0);

      // set up pins and start with breaks on
      digitalWrite(forward_ch2_pin, LOW);
      digitalWrite(reverse_ch2_pin, LOW);
      pinMode(forward_ch2_pin, OUTPUT);
      pinMode(reverse_ch2_pin, OUTPUT);
      // Initialize motor PWM (5 kHz)
      ledcSetup(RIGHT_MOTOR_PWM_CHANNEL, MOTOR_PWM_FREQUENCY, 8); // Channel 0, 5 kHz frequency, 8-bit resolution
      ledcAttachPin(enable_ch2_pin, RIGHT_MOTOR_PWM_CHANNEL);     // Attach motorPin to PWM channel 0
      ledcWrite(RIGHT_MOTOR_PWM_CHANNEL, 0);

      break;

    default:
      // do not set-up
      break;
    }
    Motor_initialised = true;
  }
}

void Motor_Drive(int speed, int balance)
// speed -100 -- +100, negative for reverse. Unsing linear to pwm,  = 70 (%) means about half the power
{
  if (!Motor_initialised)
  {
    Motor_Begin();
  }
  xSemaphoreTake(Motor_Mutex,portMAX_DELAY);
  if (Motor_motorType == DIFFERENTIAL)
  {
    // drive both motors the same.
    // to implement differential drive based on steering
    if (0 == speed)
    {
      ledcWrite(LEFT_MOTOR_PWM_CHANNEL, 0);
      ledcWrite(RIGHT_MOTOR_PWM_CHANNEL, 0);
      // free wheel is automatic since thre enable signal is missing we do not need to change
      // the FORWARD or REVERSE pins
    }
    if (speed > 100)
      speed = 100;
    if (speed < -100)
      speed = -100;
    if (speed > 0)
    {
      digitalWrite(FORWARD_ch1_PIN, HIGH);
      digitalWrite(REVERSE_ch1_PIN, LOW);
      ledcWrite(LEFT_MOTOR_PWM_CHANNEL, (int)((long)speed * 255L / 100));
      digitalWrite(FORWARD_ch2_PIN, HIGH);
      digitalWrite(REVERSE_ch2_PIN, LOW);
      ledcWrite(RIGHT_MOTOR_PWM_CHANNEL, (int)((long)speed * 255L / 100));
    }
    if (speed < 0)
    {

      digitalWrite(FORWARD_ch1_PIN, LOW);
      digitalWrite(REVERSE_ch1_PIN, HIGH);
      ledcWrite(LEFT_MOTOR_PWM_CHANNEL, abs((int)((long)speed * 255L / 100)));
      digitalWrite(FORWARD_ch2_PIN, LOW);
      digitalWrite(REVERSE_ch2_PIN, HIGH);
      ledcWrite(RIGHT_MOTOR_PWM_CHANNEL, abs((int)((long)speed * 255L / 100)));
    }
  }
  else // MOTORTYPE IS SINGLE meaning that right and left are driven the same = only one connection to drive motors
  {
    // SINGLE
    // bvalance to be ignored in single mode
    if (0 == speed)
    {
      ledcWrite(LEFT_MOTOR_PWM_CHANNEL, 0);
      // free wheel is automatic since thre enable signal is missing we do not need to change
      // the FORWARD or REVERSE pins
    }
    if (speed > 100)
      speed = 100;
    if (speed < -100)
      speed = -100;
    if (speed > 0)
    {
      digitalWrite(FORWARD_ch1_PIN, HIGH);
      digitalWrite(REVERSE_ch1_PIN, LOW);
      ledcWrite(LEFT_MOTOR_PWM_CHANNEL, (int)((long)speed * 255L / 100));
    }
    if (speed < 0)
    {

      digitalWrite(FORWARD_ch1_PIN, LOW);
      digitalWrite(REVERSE_ch1_PIN, HIGH);
      ledcWrite(LEFT_MOTOR_PWM_CHANNEL, abs((int)((long)speed * 255L / 100)));
    }
  }
  xSemaphoreGive(Motor_Mutex);
}

void Motor_Brake(int power, int balance)
// power: 0-100.
// balance: -100 -- +100, indicates power balance.  - for left. -20 indicates, the braking power on right, 20% less for left.

{
  if (!Motor_initialised)
  {
    Motor_Begin();
  }
  xSemaphoreTake(Motor_Mutex,portMAX_DELAY);
  if (DIFFERENTIAL == Motor_motorType)
  {

    digitalWrite(FORWARD_ch1_PIN, LOW);
    digitalWrite(REVERSE_ch1_PIN, LOW);
    ledcWrite(LEFT_MOTOR_PWM_CHANNEL, abs((int)((long)power * 255L / 100)));
    digitalWrite(FORWARD_ch2_PIN, LOW);
    digitalWrite(REVERSE_ch2_PIN, LOW);
    ledcWrite(RIGHT_MOTOR_PWM_CHANNEL, abs((int)((long)power * 255L / 100)));
  }
  else
  {

    digitalWrite(FORWARD_ch1_PIN, LOW);
    digitalWrite(REVERSE_ch1_PIN, LOW);
    ledcWrite(LEFT_MOTOR_PWM_CHANNEL, (int)((long)power * 255L / 100));
  }
  xSemaphoreGive(Motor_Mutex);
}
