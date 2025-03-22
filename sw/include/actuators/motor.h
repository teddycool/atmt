// hearderfile for motor control

#ifndef MOTOR_H
#define MOTOR_H

#include <config.h>

  void Motor_Begin(void);

  void Motor_Drive(int speed, int balance = 0);
  //__speed__ between -100 for full reverse to 100 representing full speed ahead.
  // speed 0 0 means idle = freewheeling
  // if PWM is not supported any positive number means full forward, and any negative number means full reverse
  //balance to be documented when implemented, for now just ignore
 
  void Motor_Brake(int power, int balance = 0);

#endif
