#ifndef ATMIO_H
#define ATMIO_H  

//Hardware-settings depending on the used hardware

#define USE_SERVO 0 //Use servo for steering 0 meeans no servo and a separate motor driver is used
#define USE_LIGHT 1 
#define USE_MPU 1
#define USE_COMPASS 1

#define USE_MQTT 1


// Define pin-name and GPIO#. Pin # are depending on HW version
// Naming is FIXED

//Hardware version 2 & 3:
//-----------------------

// Motor
#define M1E_PIN 2
#define M1F_PIN 4
#define M1R_PIN 5

// Steering (without servo)
#define SENABLE 33
#define SRIGHT 27
#define SLEFT 23

// US sensors
// Ultrasound number and pins for PMS

// #define NUM_SENSORS 4
// #define TRIGGER_PIN1 16
// #define ECHO_PIN1 34
// #define TRIGGER_PIN2 17
// #define ECHO_PIN2 35
// #define TRIGGER_PIN3 26
// #define ECHO_PIN3 25
// #define TRIGGER_PIN4 19
// #define ECHO_PIN4 18

// Ultrasound number and pins for ATMT
#define NUM_SENSORS 4
#define TRIGGER_PIN1 16 //Front
#define ECHO_PIN1 34
#define TRIGGER_PIN2 17 //Right
#define ECHO_PIN2 35
#define TRIGGER_PIN3 26 //Left
#define ECHO_PIN3 25
#define TRIGGER_PIN4 19 //Rear
#define ECHO_PIN4 18

#

// Light (Neopixel)
#define LIGHT_PIN 14
#define LIGHT_NUM_LEDS 4

#define SDA_PIN 21
#define SCL_PIN 22

#endif