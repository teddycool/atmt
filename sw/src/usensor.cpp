#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include "usensor.h"
#include "setget.h"

struct TaskParams
{
    uint8_t TRIG;
    uint8_t ECHO;
    VarNames name;
};

public:
Usensor::Usensor(uint8_t trig, uint8_t echo, VarNames name) : ECHO(echo), TRIG(trig), NAME(name)
{
    // Serial.println("US pin setup...");
    digitalWrite(TRIG, LOW);
    pinMode(TRIG, OUTPUT);
    pinMode(ECHO, INPUT);
    TaskParams *params = new TaskParams{TRIG, ECHO, name};
    xTaskCreate(TriggerTask, "testsetup", 2000, params, 1, NULL);
}

static void Usensor::TriggerTask(void *pvParameters)
{
    //  Serial.println("Distance sensor test...");
    // Initialize trigger and echo pins
    TaskParams *params = (TaskParams *)pvParameters;
    pinMode(params->TRIG, OUTPUT);
    pinMode(params->ECHO, INPUT);
    pinMode(triggerPins[i], OUTPUT);
    pinMode(echoPins[i], INPUT);

    // Attach an interrupt to the echo pin
    attachInterrupt(digitalPinToInterrupt(echoPins[i]), echoInterrupt, CHANGE);

    for (;;)
    {
        // Send a 10 microsecond pulse to start the sensor
        digitalWrite(params->TRIG, HIGH);
        delayMicroseconds(10);
        digitalWrite(params->TRIG, LOW);

        // Wait for 100 ms before polling the next sensor
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void echoInterrupt()
{
    int i = currentSensor;
    if (digitalRead(echoPins[i]) == HIGH)
    {
        // The echo pin went from LOW to HIGH: start timing
        startTime[i] = micros();
    }
    else
    {
        // The echo pin went from HIGH to LOW: stop timing and calculate distance
        long travelTime = micros() - startTime[i];
        switch (i)
        {
        case 0:
            globalVar_set(rawDistFront, travelTime / 29 / 2);
            break;
        case 1:
            globalVar_set(rawDistRight, travelTime / 29 / 2);
            break;
        case 2:
            globalVar_set(rawDistLeft, travelTime / 29 / 2);
            break;
        case 3:
            globalVar_set(rawDistBack, travelTime / 29 / 2);
            break;
        }
        // distance[i] = travelTime / 29 / 2;
    }
}
