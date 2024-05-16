//hearderfile for motor control

#ifndef DRIVE_H
#define DRIVE_H    

#include "motor.h"

class Drive
{
public:
  Drive(Motor motor );  // declare default constructor with controlpin input

private:
  Motor motor;

public:
  void Forward(int speed);
  void Reverse(int speed);
  void Stop();
};

#endif