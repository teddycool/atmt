//hearderfile for ultrasonic distance 
#ifndef USENSOR_H
#define USENSOR_H  

#include "setget.h"

class Usensor
{
public:
  Usensor(uint8_t, uint8_t,VarNames);  
    // declare default constructor with echo and trigger pins, 
    //the varname is where the values should be stored

private:
  //float distance;
  const uint8_t ECHO;
  const uint8_t TRIG;
  const VarNames name;

public: 
  //void SetUp();
  //float GetDistance();
};

#endif
