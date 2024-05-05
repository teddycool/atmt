#include <Arduino.h>
#include "setget.h"


#define NUM_SENSORS 2
#define TRIGGER_PIN 32
#define ECHO_PIN 33
#define TRIGGER_PIN2 25
#define ECHO_PIN2 26

volatile long distance[NUM_SENSORS];
volatile long startTime[NUM_SENSORS];
volatile int currentSensor = 0;

int triggerPins[NUM_SENSORS] = {32, 25};
int echoPins[NUM_SENSORS] = {33, 26};


void echoInterrupt() {
  int i = currentSensor;
  if (digitalRead(echoPins[i]) == HIGH) {
    // The echo pin went from LOW to HIGH: start timing
    startTime[i] = micros();
  } else {
    // The echo pin went from HIGH to LOW: stop timing and calculate distance
    long travelTime = micros() - startTime[i];
    globalVar_set(rawDistLeft,travelTime/29/2);
    //distance[i] = travelTime / 29 / 2;
  }
}


void pollSensors(void *pvParameters) {
  for (;;) {
    for (int i = 0; i < NUM_SENSORS; i++) {
      currentSensor = i;

      // Initialize trigger and echo pins
      pinMode(triggerPins[i], OUTPUT);
      pinMode(echoPins[i], INPUT);

      // Attach an interrupt to the echo pin
      attachInterrupt(digitalPinToInterrupt(echoPins[i]), echoInterrupt, CHANGE);

      // Send a 10 microsecond pulse to start the sensor
      digitalWrite(triggerPins[i], HIGH);
      delayMicroseconds(10);
      digitalWrite(triggerPins[i], LOW);

      // Wait for 100 ms before polling the next sensor
      vTaskDelay(pdMS_TO_TICKS(50));
    }
  }
}




void setup() {
  // Initialize serial communication for debugging
  Serial.begin(52600);
  globalVar_init();
  // Initialize trigger and echo pins
  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Attach an interrupt to the echo pin
  attachInterrupt(digitalPinToInterrupt(ECHO_PIN), echoInterrupt, CHANGE);

  // Create a task for polling the sensors
  xTaskCreate(
    pollSensors,          // Task function
    "PollSensors",        // Task name
    1000,                 // Stack size (in words, not bytes)
    NULL,                 // Task input parameter
    2,                    // Task priority
    NULL                  // Task handle
  );
}

void loop() {
  // Empty. All the work is done in tasks.
    // Print the distance for debugging
    Serial.print("Sensor ");
    Serial.print(0);
    Serial.print(": ");
    //Serial.print(distance[currentSensor]);
    Serial.print(globalVar_get(rawDistLeft));
    Serial.println(" cm");
    vTaskDelay(pdMS_TO_TICKS(350));
}

