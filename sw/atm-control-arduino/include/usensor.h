//hearderfile for ultrasonic distance 
#ifndef USENSOR_H
#define USENSOR_H  

class Usensor
{
public:
  Usensor(uint8_t, uint8_t);  // declare default constructor with echo and trigger pins

private:
  float distance;
  const uint8_t ECHO;
  const uint8_t TRIG;

public: 
  void SetUp();
  float GetDistance();
};

#endif
