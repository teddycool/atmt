#include "strategy.h"
#include <vector>

Strategy::Strategy(Steer steer, Drive drive) : steer(steer), drive(drive){    
 
}

float GetCleanDist(std::vector<float> &vec)
{
  float res = 0.0;
  for (int i = 0; i < 3; i++)
  {
    res += vec.at(i);
  }
  return res / 3;
}

bool objectInRange(std::vector<float> &vec, int rng )
{
  float dist = GetCleanDist(vec);
  return dist > 0 && dist < rng;
}

void masterStrat() {
  drive.Forward(1);
}

// void masterStrat(int sideoffset)
// {
//   if (!objectInRange(frontDist, 30))
//   {
//     drive.Forward(1);
//     // light.HeadLight();
//   }
//   else
//   {   
//      if (!objectInRange(rearDist, 30)){ 
//     drive.Reverse(1);
//     // light.BrakeLight();
//      }
//      else{
//       drive.Stop();
//      }
//   }

//   steer.Stop();

//   if (objectInRange(leftDist,sideoffset))
//   {
//     steer.Right();
//   }
//   if (objectInRange(rightDist, sideoffset))
//   {
//     steer.Left();
//   }
    
// }


void Strategy::Execute(std::vector<float> &frontDistVec, std::vector<float> &rearDistVec, std::vector<float> &leftDistVec, std::vector<float> &rightDistVec){
  frontDist = GetCleanDist(frontDistVec);
  rearDist = GetCleanDist(rearDistVec);
  leftDist = GetCleanDist(leftDistVec);
  rightDist = GetCleanDist(rightDistVec);

  // masterStrat(45);
}
