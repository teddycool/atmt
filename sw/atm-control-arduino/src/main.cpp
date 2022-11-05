#include <Arduino.h>

#include "motor.h"
#include "usensor.h"
#include "light.h"
#include "dynamics.h"

//Define pins name and GPIO#
#define M1E_PIN 2
#define M1F_PIN 4
#define M1R_PIN 5

#define SENABLE 33
#define SRIGHT 27
#define SLEFT 23

#define TFRONT 16  //ECHO1
#define EFRONT 34

#define TLEFT 26  //ECHO3
#define ELEFT 25

#define TREAR 19  //ECHO4
#define EREAR 18

#define TRIGHT 17  //ECHO2
#define ERIGHT 35


Dynamics dynamics;

Usensor leftdistance(TLEFT,ELEFT); 
Usensor rightdistance(TRIGHT,ERIGHT); 
Usensor frontdistance(TFRONT,EFRONT); 
Usensor reardistance(TREAR,EREAR); 

Motor motor(M1E_PIN, M1F_PIN, M1R_PIN);
Motor steering(SENABLE, SLEFT, SRIGHT);

Light light;

void setup() {
  //Serial Port begin
  Serial.begin (9600);
  Serial.println("----------");
  Serial.println("ATMT started!");
  frontdistance.SetUp();
  reardistance.SetUp();
  rightdistance.SetUp();
  leftdistance.SetUp();
  motor.SetUp();
  steering.SetUp();
  light.SetUp();  
  dynamics.SetUp();
  
}

void loop() {
  steering.Start();
  motor.Start();
  light.Test();
  dynamics.Update();
  Serial.println("Front: " + String(frontdistance.GetDistance()));  
  Serial.println("Rear: " + String(reardistance.GetDistance()));  
  Serial.println("Right: " + String(rightdistance.GetDistance()));  
  Serial.println("Left: " + String(leftdistance.GetDistance())); 
  Serial.println("AccX: " + String(dynamics.GetAccX())); 
  Serial.println("AccY: " + String(dynamics.GetAccY())); 
  Serial.println("AccZ: " + String(dynamics.GetAccZ())); 
  Serial.println("GyroX: " + String(dynamics.GetGyroX()));
  Serial.println("GyroY: " + String(dynamics.GetGyroY()));
  Serial.println("GyroZ: " + String(dynamics.GetGyroZ()));
  delay(1000);
  steering.Reverse();
  motor.Reverse();
  light.Off();
  delay(1500);
}