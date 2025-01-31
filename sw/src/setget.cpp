//#include "FreeRTOS.h"
#include "setget.h"
#include <FreeRTOS.h>
#include <semphr.h>


// Define a struct to encapsulate each variable
typedef struct {
    volatile int value;
    volatile long lastUpdate;
    xSemaphoreHandle sem;
} GlobalVar;

// Create an array of global variables
GlobalVar vars[NUM_VARS];


void globalVar_init() {
    // Initialize the semaphores for each variable
    for (int i = 0; i < NUM_VARS; i++) {
        vars[i].sem = xSemaphoreCreateMutex();
    }
}

void globalVar_set(VarNames varName, int value) {
    xSemaphoreTake(vars[varName].sem, portMAX_DELAY);
    vars[varName].value = value;
    vars[varName].lastUpdate = xTaskGetTickCount() * portTICK_PERIOD_MS;
    xSemaphoreGive(vars[varName].sem);
}

int globalVar_get(VarNames varName) {
    int value;
    xSemaphoreTake(vars[varName].sem, portMAX_DELAY);
    value = vars[varName].value;
    xSemaphoreGive(vars[varName].sem);
    return value;
}

int globalVar_get(VarNames varName, long *age) {
    int value;
    xSemaphoreTake(vars[varName].sem, portMAX_DELAY);
    value = vars[varName].value;
    *age = xTaskGetTickCount() * portTICK_PERIOD_MS - vars[varName].lastUpdate;
    xSemaphoreGive(vars[varName].sem);
    return value;
}
