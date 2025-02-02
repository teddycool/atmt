#include <Arduino.h>
#include "actuators/steer.h"

Steer::Steer(Motor motor) : motor(motor){    
}

void Steer::Right(){
  Serial.println("Steering right");
  motor.Reverse();
}

void Steer::Left(){
  Serial.println("Steering left");
  motor.Start();
}

void Steer::Stop(){
  Serial.println("Steering stopped");
  motor.Stop();
}


