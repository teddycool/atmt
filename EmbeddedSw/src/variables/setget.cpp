// #include "FreeRTOS.h"
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
    xSemaphoreHandle sem;
} GlobalVar;

// Create an array of global variables
GlobalVar vars[NUM_VARS];

void globalVar_init()
{
    // Initialize the semaphores for each variable
    for (int i = 0; i < NUM_VARS; i++)
    {
        vars[i].sem = xSemaphoreCreateMutex();
        vars[i].total = 0;
        vars[i].prev_value = 9999;
        vars[i].org_value = 9999;
    }
}

void globalVar_set(VarNames varName, long value)
{
    xSemaphoreTake(vars[varName].sem, portMAX_DELAY);
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

long globalVar_get(VarNames varName)
{
    long value;
    xSemaphoreTake(vars[varName].sem, portMAX_DELAY);
    value = vars[varName].value;
    xSemaphoreGive(vars[varName].sem);
    return value;
}

long globalVar_get_total(VarNames varName)
{
    long value;
    xSemaphoreTake(vars[varName].sem, portMAX_DELAY);
    value = vars[varName].total;
    xSemaphoreGive(vars[varName].sem);
    return value;
}

void globalVar_reset_total(VarNames varName)
{
    xSemaphoreTake(vars[varName].sem, portMAX_DELAY);
    vars[varName].total = 0;
    xSemaphoreGive(vars[varName].sem);
}

long globalVar_get_delta(VarNames varName)
{
    long value;
    xSemaphoreTake(vars[varName].sem, portMAX_DELAY);
    value = vars[varName].value - vars[varName].prev_value;
    xSemaphoreGive(vars[varName].sem);
    return value;
}

long globalVar_get_TOT_delta(VarNames varName)
{
    long value;
    xSemaphoreTake(vars[varName].sem, portMAX_DELAY);
    value = vars[varName].value - vars[varName].org_value;
    xSemaphoreGive(vars[varName].sem);
    return value;
}

long globalVar_get(VarNames varName, long *age)
{
    long value;
    xSemaphoreTake(vars[varName].sem, portMAX_DELAY);
    value = vars[varName].value;
    *age = xTaskGetTickCount() * portTICK_PERIOD_MS - vars[varName].lastUpdate;
    xSemaphoreGive(vars[varName].sem);
    return value;
}
