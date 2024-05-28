//hearderfile for motor control

#ifndef MOTOR_H
#define MOTOR_H    

class Motor
{
public:
  Motor(uint8_t, uint8_t , uint8_t );  // declare default constructor with controlpin input

private:
  const uint8_t ME;  
  const uint8_t MF;
  const uint8_t MB;
  uint8_t speed;    //pwm % for enable

public:
  void SetUp();
  void Stop();
  void Start();
  void Reverse();
};

#endif
