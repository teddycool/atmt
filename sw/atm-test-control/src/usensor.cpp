#include <Arduino.h>
#include "usensor.h"


Usensor::Usensor(uint8_t trig, uint8_t echo) : ECHO(echo), TRIG(trig)
{
  
}

void Usensor::SetUp(){
    Serial.println("US pin setup...");
    pinMode(TRIG, OUTPUT);
    pinMode(ECHO, INPUT);
}

float Usensor::GetDistance(){    
//  Serial.println("Distance sensor test...");
  digitalWrite(TRIG, LOW);
  delayMicroseconds(10);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(20);
 // Serial.println("Trigger puls sent");
  digitalWrite(TRIG, LOW);
  long duration = pulseIn(ECHO, HIGH, 10000);
//  Serial.println("Received echo");
//  Serial.print("Duration: ");
//  Serial.print(duration);
//  Serial.println();
  // // Convert the time into a distance
  float distance = (duration/2) / 29.1;     // Divide by 29.1 or multiply by 0.0343'
 // Serial.print("Distance is ");
 // Serial.print(distance);
 // Serial.println(" cm");

  return distance;
  }