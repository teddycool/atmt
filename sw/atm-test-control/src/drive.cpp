#include <Arduino.h>
#include "drive.h"

Drive::Drive(Motor motor) : motor(motor){    
}

void Drive::Forward(int speed){
  Serial.println("Motor forwarding");
  motor.Start();
}

void Drive::Reverse(int speed){
  Serial.println("Motor reversing");
  motor.Reverse();
}

void Drive::Stop(){
  Serial.println("Motor stopped");
  motor.Stop();
}
