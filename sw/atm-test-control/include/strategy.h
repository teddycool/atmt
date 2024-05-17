//hearderfile for motor control
#include <vector>
#include "drive.h"
#include "steer.h"

#ifndef STRATEGY_H
#define STRATEGY_H    

class Strategy
{
public:
  Strategy(Steer steer, Drive drive);

private:
  Steer steer;
  Drive drive;
  float frontDist;
  float rearDist;
  float leftDist;
  float rightDist;

public:
  void Execute(std::vector<float> &frontDist, std::vector<float> &rearDist, std::vector<float> &leftDist,std::vector<float> &rightDist);
};

#endif
