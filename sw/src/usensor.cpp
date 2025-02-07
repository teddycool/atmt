#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <usensor.h>
#include <setget.h>

#define MAX_SENSORS 10

struct sensors
{
    uint8_t TRIG;
    uint8_t ECHO;
    VarNames NAME;
};

volatile sensors sensor[MAX_SENSORS];

volatile int num_sensors = 0;
volatile int current_sensor = 0;
volatile bool pulse_active = false; // indicate a pulse has been sent and no echo yet received.
volatile long startTime;

void echoInterrupt()
{
    int i = current_sensor;
    if (digitalRead(sensor[current_sensor].ECHO) == HIGH)
    {
        Serial.print(" S");
        Serial.print(current_sensor);
        Serial.println("u ");
        pulse_active = true;

        // The echo pin went from LOW to HIGH: start timing
        startTime = micros();
    }
    else
    {
         Serial.print(" S");
        Serial.print(current_sensor);
        Serial.println("d ");
        //long tmp = micros() - startTime ;
        // The echo pin went from HIGH to LOW: stop timing and calculate distance
        pulse_active = false;
        long travelTime = micros() - startTime; // need to make this wrap around safe by the int - uint trick
        // distance[i] = travelTime / 29 / 2;
        globalVar_set(sensor[current_sensor].NAME,travelTime / 29 / 2);
    }
}

static void TriggerTask(void *params)
{
    //  Serial.println("Distance sensor test...");
    // Initialize trigger and echo pins

    // Attach an interrupt to the echo pin
    // attachInterrupt(digitalPinToInterrupt(echoPins[i]), echoInterrupt, CHANGE);

    for (;;)
    {
        Serial.print(">");
        if (num_sensors > 0)
            if (pulse_active)
                vTaskDelay(pdMS_TO_TICKS(50)); // we have not yet reveived an echo, wait one more period, then go anyway
        current_sensor++;
        if (current_sensor >= num_sensors)
            current_sensor = 0;
        {
            // Initialize trigger and echo pins
            pinMode(sensor[current_sensor].TRIG, OUTPUT);
            pinMode(sensor[current_sensor].ECHO, INPUT);

            // Attach an interrupt to the echo pin
            attachInterrupt(digitalPinToInterrupt(sensor[current_sensor].ECHO), echoInterrupt, CHANGE);

            // Send a 10 microsecond pulse to start the sensor
            pulse_active = true;
            digitalWrite(sensor[current_sensor].TRIG, HIGH);
            delayMicroseconds(10);
            digitalWrite(sensor[current_sensor].TRIG, LOW);
        }
        // Wait for 50 ms before polling the next sensor
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

Usensor::Usensor()
{
    // start the task that regularly will trigger the opened sensors
    xTaskCreate(TriggerTask, "testsetup", 2000, NULL, 1, NULL);
}

int Usensor::open(uint8_t trig, uint8_t echo, VarNames name)
{
    if (num_sensors < MAX_SENSORS)
    {
        Serial.print("+");
        sensor[current_sensor].TRIG = trig;
        sensor[current_sensor].ECHO = echo;
        sensor[current_sensor].NAME = name;
        num_sensors++;
        return num_sensors;
    }
}

