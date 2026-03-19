#include "variables/setget.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

// Define a struct to encapsulate each variable
typedef struct
{
    volatile long value;
    volatile long total;
    volatile long org_value;
    volatile long prev_value;
    volatile long lastUpdate;
    SemaphoreHandle_t sem;  // Updated from deprecated xSemaphoreHandle
} GlobalVar;

// Create an array of global variables
GlobalVar vars[NUM_VARS];

void globalVar_init()
{
    // Initialize the semaphores for each variable with error checking
    for (int i = 0; i < NUM_VARS; i++)
    {
        vars[i].sem = xSemaphoreCreateMutex();
        if (vars[i].sem == NULL) {
            // Handle semaphore creation failure - use a simple approach
            vars[i].sem = NULL;
        }
        vars[i].total = 0;
        vars[i].prev_value = 9999;
        vars[i].org_value = 9999;
        vars[i].value = 0;
        vars[i].lastUpdate = 0;
    }
}

void globalVar_set(VarNames varName, long value)
{
    if (varName >= NUM_VARS || vars[varName].sem == NULL) {
        return; // Safety check
    }
    
    // Use timeout to prevent deadlock
    if (xSemaphoreTake(vars[varName].sem, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (vars[varName].org_value == 9999)
            vars[varName].org_value = value;
        if (vars[varName].prev_value == 9999)
        {
            vars[varName].prev_value = value;
        }
        vars[varName].prev_value = vars[varName].value;
        vars[varName].value = value;
        vars[varName].total += value;
        vars[varName].lastUpdate = xTaskGetTickCount() * portTICK_PERIOD_MS;
        xSemaphoreGive(vars[varName].sem);
    }
    // If timeout, just skip the update to avoid deadlock
}

long globalVar_get(VarNames varName)
{
    if (varName >= NUM_VARS || vars[varName].sem == NULL) {
        return 0; // Safety check
    }
    
    long value = 0;
    // Use timeout to prevent deadlock
    if (xSemaphoreTake(vars[varName].sem, pdMS_TO_TICKS(10)) == pdTRUE) {
        value = vars[varName].value;
        xSemaphoreGive(vars[varName].sem);
    }
    return value;
}

long globalVar_get_total(VarNames varName)
{
    if (varName >= NUM_VARS || vars[varName].sem == NULL) {
        return 0; // Safety check
    }
    
    long value = 0;
    if (xSemaphoreTake(vars[varName].sem, pdMS_TO_TICKS(10)) == pdTRUE) {
        value = vars[varName].total;
        xSemaphoreGive(vars[varName].sem);
    }
    return value;
}

void globalVar_reset_total(VarNames varName)
{
    if (varName >= NUM_VARS || vars[varName].sem == NULL) {
        return; // Safety check
    }
    
    if (xSemaphoreTake(vars[varName].sem, pdMS_TO_TICKS(10)) == pdTRUE) {
        vars[varName].total = 0;
        xSemaphoreGive(vars[varName].sem);
    }
}

long globalVar_get_delta(VarNames varName)
{
    if (varName >= NUM_VARS || vars[varName].sem == NULL) {
        return 0; // Safety check
    }
    
    long value = 0;
    if (xSemaphoreTake(vars[varName].sem, pdMS_TO_TICKS(10)) == pdTRUE) {
        value = vars[varName].value - vars[varName].prev_value;
        xSemaphoreGive(vars[varName].sem);
    }
    return value;
}

long globalVar_get_TOT_delta(VarNames varName)
{
    if (varName >= NUM_VARS || vars[varName].sem == NULL) {
        return 0; // Safety check
    }
    
    long value = 0;
    if (xSemaphoreTake(vars[varName].sem, pdMS_TO_TICKS(10)) == pdTRUE) {
        value = vars[varName].value - vars[varName].org_value;
        xSemaphoreGive(vars[varName].sem);
    }
    return value;
}

long globalVar_get(VarNames varName, long *age)
{
    if (varName >= NUM_VARS || vars[varName].sem == NULL) {
        if (age) *age = 99999; // Very old age to indicate error
        return 0; // Safety check
    }
    
    long value = 0;
    // Use timeout to prevent deadlock
    if (xSemaphoreTake(vars[varName].sem, pdMS_TO_TICKS(10)) == pdTRUE) {
        value = vars[varName].value;
        if (age) {
            *age = xTaskGetTickCount() * portTICK_PERIOD_MS - vars[varName].lastUpdate;
        }
        xSemaphoreGive(vars[varName].sem);
    } else {
        // Timeout occurred
        if (age) *age = 99999; // Very old age to indicate timeout
    }
    return value;
}
