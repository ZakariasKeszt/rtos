#ifndef _MAIN_H
#define _MAIN_H

#include <stdint.h>
#include "task.h"

#define NUM_TASKS 3

/* Task priorities: higher number = higher priority (idle task = 0) */
#define PRIO_MODBUS     3
#define PRIO_SENSOR     2
#define PRIO_HEARTBEAT  1

#define SENSOR_PERIOD_TICKS     100     /* 100 ms at 1 kHz SysTick */
#define HEARTBEAT_PERIOD_TICKS  500     /* 500 ms */
#define SENSOR_QUEUE_LEN        8

/* Latest sensor values, shared between tasks and protected by a mutex */
typedef struct {
    uint32_t temperature;
    uint32_t humidity;
    uint32_t sample_count;
} sensor_data_t;

extern TCB_t tasks[NUM_TASKS];
extern uint32_t task_stacks[NUM_TASKS][TASK_STACK_SIZE];

void modbus_task(void);
void sensor_task(void);
void heartbeat_task(void);
void Error_Handler(void);

#endif
