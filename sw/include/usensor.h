//hearderfile for ultrasonic distance 
#ifndef USENSOR_H
#define USENSOR_H  

#include <setget.h>

class Usensor
{
public:
  Usensor();  
    // declare default constructor with echo and trigger pins, 
    //the varname is where the values should be stored

  int Usensor::open(uint8_t trig, uint8_t echo,VarNames name);
    //add a sensor to the list of sensors measured. 

  //int read(VarNames);

private:
  //float distance;
  /*const uint8_t ECHO;
  const uint8_t TRIG;
  const VarNames NAME; */
  //static void TriggerTask();

public: 
  //void SetUp();
  //float GetDistance();
  //void SetUp();
  //float GetDistance();
};

#endif
