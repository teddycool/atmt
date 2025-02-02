#ifndef LIGHT_H
#define LIGHT_H    

class Light
{


public:  
  void SetUp();
  void Off();
  void Set(int, uint8_t);
  void Test();
  void HeadLightOn();
  void HeadLightOff();
  void BrakeLightOn();
  void BrakeLightOff();


};

#endif