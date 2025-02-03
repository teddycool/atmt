#ifndef GLOBAL_VAR_H
#define GLOBAL_VAR_H


// Define the number of variables
//#define NUM_VARS 4



// Create a lookup table for the variable names
typedef enum {
    rawDistLeft,  //in cm
    rawDistFront, 
    rawDistRight, 
    rawDistBack,
    rawMagX,
    rawMagY,
    rawMagZ,
    rawAccX,
    rawAccY,
    rawAccZ,
    rawTemp,
    rawGyX,  // (1/132 degrees per second)
    rawGyY,  // (1/132 degrees per second)
    rawGyZ,  // 1/132 degrees per second
    rawLidarFront,   // mm
    calcHeading,   // 1/10 degrees
    calcSpeed,     // mm / second
    calcDistance,  // mm
    calcXpos,      // mm
    calcYpos,      // mm
    NUM_VARS
} VarNames;

// Function prototypes
void globalVar_init(void);
void globalVar_set(VarNames varName, int value);
int globalVar_get(VarNames varName);
int globalVar_get_total(VarNames varName);
void globalVar_reset_total(VarNames varName);
int globalVar_get(VarNames varName, long *age);
int globalVar_get_delta(VarNames varName);
int globalVar_get_TOT_delta(VarNames varName);

#endif // GLOBAL_VAR_H
