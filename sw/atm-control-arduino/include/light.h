//hearderfile for motor control

#ifndef LIGHT_H
#define LIGHT_H    

class Light
{
public:
  Light(uint8_t );  // declare default constructor with controlpin input

private:
  const uint8_t CPIN;  

public:
  void SetUp(int);
  void Off();
  void Set(int, uint8_t);
};

#endif