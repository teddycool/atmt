#include <Arduino.h>
#include "usensor.h"


Usensor::Usensor(uint8_t trig, uint8_t echo) : ECHO(echo), TRIG(trig)
{
  
}

void Usensor::SetUp(){
   // Serial.println("US pin setup...");
    pinMode(TRIG, OUTPUT);
    pinMode(ECHO, INPUT);
}

float Usensor::GetDistance(){    
//  Serial.println("Distance sensor test...");
 float distance = 150; 
  digitalWrite(TRIG, LOW);
  delayMicroseconds(10);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(20);
  digitalWrite(TRIG, LOW);
  long duration = pulseIn(ECHO, HIGH, 10000);
  // // Convert the time into a distance
  if (duration > 0){
  distance = (duration/2) / 29.1;     // Divide by 29.1 or multiply by 0.0343'
  }
  return distance;
  }