//hearderfile for motor control

#ifndef MOTOR_H
#define MOTOR_H  

#include <config.h>



class Motor
{
private:
  motorType_t motorType;
  /*
  const uint8_t ME;  
  const uint8_t MF;
  const uint8_t MB;
  uint8_t speed;    //pwm % for enable
  */

public:
  Motor();  // declare default constructor with controlpin input



public:
  /*
  void SetUp();
  void Stop();
  void Start();
  void Reverse();
  */

  void driving(int speed, int balance = 0); 
  //__speed__ between -100 for full reverse to 100 representing full speed ahead.
  //speed 0 0 means idle = freewheeling
  //if PWM is not supported any positive number means full forward, and any negative number means full reverse
  //any number outside of -100 to + 100 will be interpreted as 0
  //__balance__ is for the situation you can have difference power distributed to left and right side. 
  //-100 represent only power on the left, +100 represent only powe on the right
  //balance is not supported on most platforms
  
  void breaking(int power, int balance = 0);
};

#endif
