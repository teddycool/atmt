#include <task_safe_wire.h>

#include <Wire.h>
#include <assert.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

typedef struct
{
    SemaphoreHandle_t mutex;
    bool initialized;
    bool session_active;
    bool write_started;
} TaskSafeWireState;

static TaskSafeWireState task_safe_wire_state = {0};
static portMUX_TYPE task_safe_wire_setup_mux = portMUX_INITIALIZER_UNLOCKED;

static void task_safe_wire_ensure_mutex()
{
    if (task_safe_wire_state.mutex != NULL)
    {
        return;
    }

    portENTER_CRITICAL(&task_safe_wire_setup_mux);
    if (task_safe_wire_state.mutex == NULL)
    {
        task_safe_wire_state.mutex = xSemaphoreCreateRecursiveMutex();
    }
    portEXIT_CRITICAL(&task_safe_wire_setup_mux);
}

void task_safe_wire_lock()
{
    task_safe_wire_ensure_mutex();
    xSemaphoreTakeRecursive(task_safe_wire_state.mutex, portMAX_DELAY);
}

bool task_safe_wire_try_lock(uint32_t timeoutMs)
{
    task_safe_wire_ensure_mutex();
    if (task_safe_wire_state.mutex == NULL)
    {
        return false;
    }
    return xSemaphoreTakeRecursive(task_safe_wire_state.mutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
}

void task_safe_wire_unlock()
{
    xSemaphoreGiveRecursive(task_safe_wire_state.mutex);
}

void task_safe_wire_begin(uint8_t address)
{
    task_safe_wire_lock();
    if (!task_safe_wire_state.initialized)
    {
        Wire.begin();
        task_safe_wire_state.initialized = true;
    }
    if (task_safe_wire_state.session_active)
    {
        // End previous session if still active
        task_safe_wire_state.session_active = false;
        task_safe_wire_state.write_started = false;
    }
    task_safe_wire_state.session_active = true;
    task_safe_wire_state.write_started = true;
    Wire.beginTransmission(address);
}

size_t task_safe_wire_write(uint8_t value)
{
    if (!task_safe_wire_state.session_active)
        return 0;
    return Wire.write(value);
}

void task_safe_wire_restart()
{
    if (!task_safe_wire_state.session_active || !task_safe_wire_state.write_started)
        return;
    Wire.endTransmission(false);
    task_safe_wire_state.write_started = false;
}

uint8_t task_safe_wire_request_from(uint8_t address, uint8_t quantity)
{
    if (!task_safe_wire_state.session_active)
        return 0;
    return Wire.requestFrom(address, quantity);
}

int task_safe_wire_read()
{
    if (!task_safe_wire_state.session_active)
        return -1;
    return Wire.read();
}

int task_safe_wire_available()
{
    if (!task_safe_wire_state.session_active)
        return 0;
    return Wire.available();
}

uint8_t task_safe_wire_end()
{
    uint8_t result = 0;
    
    // Always unlock the mutex, even if session state is inconsistent
    if (!task_safe_wire_state.session_active)
    {
        // Still try to unlock in case mutex was left locked
        if (task_safe_wire_state.mutex != NULL)
        {
            xSemaphoreGiveRecursive(task_safe_wire_state.mutex);
        }
        return 1; // Error: no active session
    }

    if (task_safe_wire_state.write_started)
    {
        result = Wire.endTransmission();
    }

    task_safe_wire_state.session_active = false;
    task_safe_wire_state.write_started = false;
    task_safe_wire_unlock();
    return result;
}