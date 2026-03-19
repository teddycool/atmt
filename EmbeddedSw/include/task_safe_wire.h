#ifndef TASK_SAFE_WIRE_H
#define TASK_SAFE_WIRE_H

#include <Arduino.h>

void task_safe_wire_begin(uint8_t address);
void task_safe_wire_lock();
bool task_safe_wire_try_lock(uint32_t timeoutMs);
void task_safe_wire_unlock();
size_t task_safe_wire_write(uint8_t value);
void task_safe_wire_restart();
uint8_t task_safe_wire_request_from(uint8_t address, uint8_t quantity);
int task_safe_wire_read();
int task_safe_wire_available();
uint8_t task_safe_wire_end();

#endif