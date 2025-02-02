#include <Arduino.h>
#include "actuators/motor.h"

Motor::Motor(uint8_t ena, uint8_t frw, uint8_t rev) : ME(ena), MF(frw), MB(rev){    
}

void Motor::SetUp(){
    pinMode(ME, OUTPUT);
    pinMode(MF, OUTPUT);
    pinMode(MB, OUTPUT);
}

void Motor::Start(){
  Serial.println("Motor started");
  Serial.println("Forward...");
  digitalWrite(ME, HIGH);
  digitalWrite(MF, HIGH);
  digitalWrite(MB, LOW);
}

void Motor::Reverse(){
  Serial.println("Motor started");
  Serial.println("Reverse...");
  digitalWrite(ME, HIGH);
  digitalWrite(MF, LOW);
  digitalWrite(MB, HIGH);
}

void Motor::Stop(){
  Serial.println("Motor stopped");
  digitalWrite(ME, LOW);
  digitalWrite(MF, LOW);
  digitalWrite(MB, LOW);
}
