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
#define TFRONT 16 // ECHO1
#define EFRONT 34
#define TLEFT 26 // ECHO3
#define ELEFT 25
#define TREAR 19 // ECHO4
#define EREAR 18
#define TRIGHT 17 // ECHO2
#define ERIGHT 35

// Light (Neopixel)
#define LIGHT_PIN 14
#define LIGHT_NUM_LEDS 4

#define SDA_PIN 21
#define SCL_PIN 22

#endif