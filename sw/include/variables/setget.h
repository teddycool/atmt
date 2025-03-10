#ifndef GLOBAL_VAR_H
#define GLOBAL_VAR_H


// Define the number of variables
//#define NUM_VARS 4



// Create a lookup table for the variable names
typedef enum {
    zeroAx,         //the start offset that represent 0
    zeroGz,
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
    steerDirection,     //-100 to +100 wheel direction
    NUM_VARS
} VarNames;

// Function prototypes
void globalVar_init(void);
void globalVar_set(VarNames varName, long value);
long globalVar_get(VarNames varName);
long globalVar_get_total(VarNames varName);
void globalVar_reset_total(VarNames varName);
long globalVar_get(VarNames varName, long *age);
long globalVar_get_delta(VarNames varName);
long globalVar_get_TOT_delta(VarNames varName);

#endif // GLOBAL_VAR_H
