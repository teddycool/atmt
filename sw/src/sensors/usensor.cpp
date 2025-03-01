#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <sensors/usensor.h>
#include <variables/setget.h>

#define MAX_SENSORS 10

#define POLL_INTERVAL 15
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

void echoInterrupt()
{

    if (digitalRead(sensor[current_sensor].ECHO) == HIGH)
    {
        /*Serial.print(" S");
        Serial.print(current_sensor);
        Serial.println("u ");*/
        pulse_active = true;

        // The echo pin went from LOW to HIGH: start timing
        startTime = micros();
    }
    else
    {
        /*Serial.print(" S");
       Serial.print(current_sensor);
       Serial.println("d ");*/
        // long tmp = micros() - startTime ;
        //  The echo pin went from HIGH to LOW: stop timing and calculate distance
        pulse_active = false;
        long travelTime = micros() - startTime; // need to make this wrap around safe by the int - uint trick
        int distance = travelTime / 29 / 2;     // in cm
        if (distance > 199)
            distance = 199;
        globalVar_set(sensor[current_sensor].NAME, distance);
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
        // Serial.print(">");
        if (num_sensors > 0)
        {
            if (pulse_active)
            {
                // vTaskDelay(pdMS_TO_TICKS(50)); // we have not yet reveived an echo from the previous trigger,
                // set distance to 199 indicating unknown value
                globalVar_set(sensor[current_sensor].NAME, 199);
            }
            current_sensor++;
            if (current_sensor >= num_sensors)
            {
                // Making sure we iterate between the sensors we have opened, returning to the first sensor to start over again
                // maybe a for loop inside a loop would be more visual ans self explanatory?
                // It would also be possible to build a more advanced scheduling algorithm, for instance that the first
                // sensor registered would be be polled in between every other, to give it much higher resolution
                //  Could make sense for the forward sensor if running at high speeds.
                current_sensor = 0;
            }
            // Initialize trigger and echo pins
            pinMode(sensor[current_sensor].TRIG, OUTPUT);
            pinMode(sensor[current_sensor].ECHO, INPUT);

            // Attach an interrupt to the echo pin, the same task for all pins
            // as we only run one sensor at a time that works great
            attachInterrupt(digitalPinToInterrupt(sensor[current_sensor].ECHO), echoInterrupt, CHANGE);

            // Send a 10 microsecond pulse to start the sensor
            pulse_active = true;
            digitalWrite(sensor[current_sensor].TRIG, HIGH);
            delayMicroseconds(10);
            digitalWrite(sensor[current_sensor].TRIG, LOW);
        }
        // Wait for 50 ms before polling the next sensor
        vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL));
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
        sensor[num_sensors].TRIG = trig;
        sensor[num_sensors].ECHO = echo;
        sensor[num_sensors].NAME = name;
        num_sensors++;
    }
    return num_sensors;
}
