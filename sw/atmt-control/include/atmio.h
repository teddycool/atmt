#ifndef ATMIO_H
#define ATMIO_H  



// Define pin-name and GPIO#. Pin # are depending on HW version
// Naming is FIXED

//Hardware version 2:
//-------------------

// Motor
#define M1E_PIN 2
#define M1F_PIN 4
#define M1R_PIN 5

// Steering
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


#endif