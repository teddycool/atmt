#ifndef GLOBAL_VAR_H
#define GLOBAL_VAR_H


// Define the number of variables
//#define NUM_VARS 4



// Create a lookup table for the variable names
typedef enum {
    vehicle_motorType,
    vehicle_steerType,
    vehicle_steerServoMin,
    vehicle_steerServoMax,
    vehicle_steerServoOffset,
    vehicle_steerServoRerverse,
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
    calcGyZ, // 1/10 degrees per second
    rawLidarFront,   // mm
    calcGyroRate,    // degrees/second
    calcGyroHeading,   // degrees, 
    calcAx,             // mm/s2
    calcMagneticHeading,  // degrees, as compass 0= north
    calcSpeed,     // mm / second
    calcDistance,  // mm
    calcXpos,      // mm
    calcYpos,      // mm
    currentSteer,     //-100 to +100 wheel direction
    //>>> Robot - DRIVER
    driver_driverActivity,   //  indicator of what the robot driver is doing. Is ready when None, or Error state.
    driver_desired_heading,  //  turn until this heading has been obtained, then continue until distance.
    driver_desired_turn,  //  turn until this relative direction is obtained, then conitua until distance reaced
    driver_desired_speed,  // attain this speed until distance reached
    driver_desired_distance,   // stop when this distance has been reacherd
    //<<< Robot - DRIVER
    //>>> Actuator Light Services
    light_HAZARD,
    NUM_VARS
} VarNames;

// Function prototypes
void Setget_Begin(void);
void Setget_Set(VarNames varName, long value);
long Setget_Get(VarNames varName);
long Setget_Get(VarNames varName, long *age);
long Setget_GetTotal(VarNames varName);
void Setget_ResetTotal(VarNames varName);
long Setget_GetDelta(VarNames varName);
long Setget_GetTotalDelta(VarNames varName);

#endif // GLOBAL_VAR_H
