#include <Arduino.h>
#include <motor.h>

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

// motorType_t MOTORTYPE;

// left or single
uint8_t enable_ch1_pin = 0;
uint8_t forward_ch1_pin = 0;
uint8_t reverse_ch1_pin = 0;

// right when differential
uint8_t enable_ch2_pin = 0;
uint8_t forward_ch2_pin = 0;
uint8_t reverse_ch2_pin = 0;

/*Motor::Motor(uint8_t ena, uint8_t frw, uint8_t rev) : ME(ena), MF(frw), MB(rev){
}*/
Motor::Motor(motorType_t motorType)
{
  MOTORTYPE = motorType;
  switch (motorType)
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

    //----- NOT YET IMPLEMENTED ------

    break;

  default:
    // do not set-up
    break;
  }
}

void Motor::driving(int speed, int balance)
{
  if (MOTORTYPE == DIFFERENTIAL)
  {
    // not yet implemented
  }
  else
  {
    // SINGLE
    // bvalance to be ignored in single mode
    if (speed == 0)
    {
      ledcWrite(LEFT_MOTOR_PWM_CHANNEL, 0);
      // free wheel is automatic since thre enable signal is missing we do not need to change
      // the FORWARD or REVERSE pins
    }
    if (speed > 100)
      return;
    if (speed < -100)
      return;
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
}

/*
void Motor::SetUp()
{

  Depreciated

  pinMode(ME, OUTPUT);
  pinMode(MF, OUTPUT);
  pinMode(MB, OUTPUT);

}*/

/*
void Motor::Start()
{
  Serial.println("Motor started");
  Serial.println("Forward...");
  digitalWrite(ME, HIGH);
  digitalWrite(MF, HIGH);
  digitalWrite(MB, LOW);
}*/

/*
void Motor::Reverse()
{
  Serial.println("Motor started");
  Serial.println("Reverse...");
  digitalWrite(ME, HIGH);
  digitalWrite(MF, LOW);
  digitalWrite(MB, HIGH);
}*/

/*
void Motor::Stop()
{
  Serial.println("Motor stopped");
  digitalWrite(ME, LOW);
  digitalWrite(MF, LOW);
  digitalWrite(MB, LOW);
}*/
