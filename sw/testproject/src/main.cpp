#include <Arduino.h>
#include <FastLED.h>

#define LED_PIN     32
#define COLOR_ORDER GRB
#define CHIPSET     WS2812B
#define NUM_LEDS    4
#define M1E_PIN 2
#define M1F_PIN 4
#define M1R_PIN 5
#define TRIG1_PIN 26
#define ECHO1_PIN 25


#define BRIGHTNESS  200


CRGB leds[NUM_LEDS];
long duration;
long cm;



void setup() {
 //Serial Port begin
  Serial.begin (9600);
  Serial.println("Pin set up Started!");
  //Define inputs and outputs
  pinMode(TRIG1_PIN, OUTPUT);
  pinMode(ECHO1_PIN, INPUT);
  pinMode(M1E_PIN, OUTPUT);
  pinMode(M1F_PIN, OUTPUT);
  pinMode(M1R_PIN, OUTPUT);

  digitalWrite(M1E_PIN, LOW);
  digitalWrite(M1F_PIN, LOW);
  digitalWrite(M1R_PIN, LOW);

  FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS);
  FastLED.setBrightness( BRIGHTNESS );
  Serial.println("Pin set up finished!");
}

void loop() {
   // The sensor is triggered by a HIGH pulse of 10 or more microseconds.
  // Give a short LOW pulse beforehand to ensure a clean HIGH pulse:
  Serial.println("Distance sensor test...");
  digitalWrite(TRIG1_PIN, LOW);
  delayMicroseconds(10);
  digitalWrite(TRIG1_PIN, HIGH);
  delayMicroseconds(20);
  Serial.println("Trigger puls sent");
  digitalWrite(TRIG1_PIN, LOW);
  // // Read the signal from the sensor: a HIGH pulse whose
  // // duration is the time (in microseconds) from the sending
  // // of the ping to the reception of its echo off of an object.
  duration = pulseIn(ECHO1_PIN, HIGH, 10000);
  Serial.println("Received echo");
  Serial.print("Duration: ");
  Serial.print(duration);
  Serial.println();
  // // Convert the time into a distance
  cm = (duration/2) / 29.1;     // Divide by 29.1 or multiply by 0.0343'
  Serial.print("Distance is ");
  Serial.print(cm);
  Serial.print(" cm");
  Serial.println();

  Serial.println("Motortest");
  Serial.println("Forward...");
  digitalWrite(M1E_PIN, HIGH);
  digitalWrite(M1F_PIN, HIGH);
  digitalWrite(M1R_PIN, LOW);
  delay(2000);
  Serial.println("Off...");
  digitalWrite(M1E_PIN, LOW);
  digitalWrite(M1F_PIN, LOW);
  digitalWrite(M1R_PIN, LOW);
  delay(1000);
  Serial.println("Reverse...");  
  digitalWrite(M1E_PIN, HIGH);
  digitalWrite(M1F_PIN, LOW);
  digitalWrite(M1R_PIN, HIGH);  
  delay(2000);
  Serial.println("Off...");
  digitalWrite(M1E_PIN, LOW);
  digitalWrite(M1F_PIN, LOW);
  digitalWrite(M1R_PIN, LOW);

  Serial.println("WS2812B test...");
  leds[0] = CRGB::Red;
  leds[1] = CRGB::Red;
  leds[2] = CRGB::Red;
  leds[3] = CRGB::Red;
  Serial.println("Red...");
  FastLED.show();
  delay(1000);
  // Now turn the LED off, then pause
  leds[0] = CRGB::Black;
  leds[1] = CRGB::Black;
  leds[2] = CRGB::Black;
  leds[3] = CRGB::Black;
  Serial.println("Off...");
  FastLED.show();
  delay(1000);
  leds[0] = CRGB::White;
  leds[1] = CRGB::White;
  leds[2] = CRGB::White;
  leds[3] = CRGB::White;
  Serial.println("White...");
  FastLED.show();
  delay(500);
}



