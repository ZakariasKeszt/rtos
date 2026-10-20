#ifndef _SCHED_H
#define _SCHED_H

#include "task.h"

#define IDLE_STACK_SIZE 64

extern TCB_t *next_task;
extern TCB_t *current_task;

void scheduler_init();
void scheduler_idle();
void scheduler_start();
void scheduler_add_task(TCB_t *tcb, uint8_t id, uint8_t priority, void (*task_func)(void), uint32_t *stack, uint32_t stack_size);
void schedule();

/* Time management */
void scheduler_tick(void);
void task_delay(uint32_t ticks);

/* Helpers used by sync.c / queue.c (call with interrupts disabled) */
void   scheduler_block_current(void *obj);
TCB_t *scheduler_wake_one(void *obj);
void   scheduler_reschedule(void);

#endif
