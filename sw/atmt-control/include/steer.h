//hearderfile for Steering control

#ifndef STEER_H
#define STEER_H    

#include "motor.h"

class Steer
{
public:
  Steer(Motor motor );  // declare default constructor with controlpin input
  

private:
  Motor motor;

public:
  void Right();
  void Left();
  void Stop();
};

#endif