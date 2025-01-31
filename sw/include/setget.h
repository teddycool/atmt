#ifndef GLOBAL_VAR_H
#define GLOBAL_VAR_H


// Define the number of variables
//#define NUM_VARS 4



// Create a lookup table for the variable names
typedef enum {
    rawDistLeft, 
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
    rawGyX,
    rawGyY,
    rawGyZ,
    calcHeading,
    NUM_VARS
} VarNames;

// Function prototypes
void globalVar_init(void);
void globalVar_set(VarNames varName, int value);
int globalVar_get(VarNames varName);
int globalVar_get(VarNames varName, long *age);

#endif // GLOBAL_VAR_H
