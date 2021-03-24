#include <Arduino.h>

#include "motor.h"
#include "usensor.h"
#include "light.h"

//Define pins name and GPIO#
#define M1E_PIN 2
#define M1F_PIN 4
#define M1R_PIN 5
#define SENABLE 33
#define SRIGHT 27
#define SLEFT 23
#define ELEFT 25
#define TLEFT 26
#define ERIGHT 16
#define TRIGHT 17
#define FLED  32


Usensor leftdistance(TLEFT,ELEFT); 
Usensor rightdistance(TRIGHT,ERIGHT); 
Motor motor(M1E_PIN, M1F_PIN, M1R_PIN);
Motor steering(SENABLE, SLEFT, SRIGHT);
Light light(FLED);

void setup() {
  //Serial Port begin
  Serial.begin (9600);
  Serial.println("ATMT started!");
  leftdistance.SetUp();
  rightdistance.SetUp();
  motor.SetUp();
  steering.SetUp();
  light.SetUp(4);  
}

void loop() {
  motor.Start();
  if (leftdistance.GetDistance() < 10 )
  {
     motor.Stop();
  }
  delay(1000);
}