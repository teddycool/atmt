#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <sensors/usensor.h>
#include <variables/setget.h>

#define MAX_SENSORS 4

#define POLL_INTERVAL 50
// time in ms between each sensor poll, remember one sensor at a time is polled
// not to interfere with each other
// do not go below 15ms (12 ms + some margin, for a 199cm measure)
// as the previous sound might not have had time to reflect
// 30-50 ms should be enought in most cases, and will drastically reduce CPU load
// but until you experience CPU overload, 15-20ms is ok.
// you can expect the longest age of a measure to be 4 times (with the standard truck set-up)
// the length of the POLL_INTERVAL
// If you need to reduce this time or have a need of more sensors
// you need to base the distance measure on laser rather than sound, or
// Accept a smaller maximum measure distance. If you go down to 5 ms for instance
// the absolute maximu you could measure would be about 80 cm, above that the measure would say 199
// also consider the time it takes to move a task back and forth to the delay queue (overhead)
// set the time to what you NEED, not what would be cool :-) Rather start with a high number
// like 50ms or even 100ms and see if/when you need more. Even with 100ms and 4 sensors you will
// still measure the distance in all direction more then twice each second!

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
volatile long startTime;  //need to implement proper wrap around for this using nsigned and signed
// ISR-safe handoff for distance measurement
volatile int lastDistance = 199;
volatile int lastDistanceSensor = -1;
volatile bool distanceReady = false;

void echoInterrupt()
{
    // Quick interrupt handler - minimize time spent here
    if (digitalRead(sensor[current_sensor].ECHO) == HIGH)
    {
        pulse_active = true;
        startTime = micros();
    }
    else
    {
        pulse_active = false;
        long travelTime = micros() - startTime;
        int distance = travelTime / 29 / 2;     // in cm
        if (distance > 199)
            distance = 199;
        // Defer globalVar_set to task context (ISR-safe)
        lastDistance = distance;
        lastDistanceSensor = current_sensor;
        distanceReady = true;
    }
}

static void TriggerTask(void *params)
{
    // Initialize - ensure we don't interfere with setup
    vTaskDelay(pdMS_TO_TICKS(1000)); // Wait 1 second for system to stabilize
    
    for (;;)
    {
        // Apply any completed distance measurement from ISR
        if (distanceReady && lastDistanceSensor >= 0 && lastDistanceSensor < num_sensors)
        {
            globalVar_set(sensor[lastDistanceSensor].NAME, lastDistance);
            distanceReady = false;
            lastDistanceSensor = -1;
        }
        if (num_sensors > 0)
        {
            // Check for timeout from previous pulse
            if (pulse_active)
            {
                // Previous pulse timed out - mark as unknown
                globalVar_set(sensor[current_sensor].NAME, 199);
                pulse_active = false;
            }
            
            // Move to next sensor
            current_sensor++;
            if (current_sensor >= num_sensors)
            {
                current_sensor = 0;
            }
            
            // Initialize pins for current sensor
            pinMode(sensor[current_sensor].TRIG, OUTPUT);
            pinMode(sensor[current_sensor].ECHO, INPUT);
            
            // Attach interrupt
            attachInterrupt(digitalPinToInterrupt(sensor[current_sensor].ECHO), echoInterrupt, CHANGE);
            
            // Send trigger pulse
            pulse_active = true;
            digitalWrite(sensor[current_sensor].TRIG, HIGH);
            delayMicroseconds(10);
            digitalWrite(sensor[current_sensor].TRIG, LOW);
        }
        
        // Wait with yielding to prevent starving other tasks
        vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL));
        taskYIELD(); // Explicitly yield to other tasks
    }
}

Usensor::Usensor()
{
    // Create task with lower priority to avoid interfering with main loop
    // Priority 1 is lower than default (usually 2-3), stack increased for safety
    if (xTaskCreate(TriggerTask, "USensorTask", 2048, NULL, 0, NULL) != pdPASS) {
        // Task creation failed - this prevents the race condition entirely
        // Could add error handling here if needed
    }
}

int Usensor::open(uint8_t trig, uint8_t echo, VarNames name)
{
    if (num_sensors < MAX_SENSORS)
    {
        Serial.print("+");
        sensor[num_sensors].TRIG = trig;
        sensor[num_sensors].ECHO = echo;
        sensor[num_sensors].NAME = name;
        num_sensors++;
    }
    return num_sensors;
}
