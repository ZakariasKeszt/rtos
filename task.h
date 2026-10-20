#ifndef _TASK_H
#define _TASK_H

#include <stdint.h>

#define TASK_STACK_SIZE   256

typedef enum {
    TASK_READY = 0,     /* runnable (also used for the task that is currently running) */
    TASK_RUNNING,
    TASK_BLOCKED,       /* waiting on a semaphore / mutex / queue (see waiting_obj) */
    TASK_SLEEPING       /* waiting for sleep_ticks to reach 0 (see task_delay) */
} task_status_t;

typedef struct tcb {
    uint32_t *sp;               /* MUST stay at offset 0: PendSV_Handler reads/writes it */
    struct tcb *next;           /* next TCB in the circular task list */

    task_status_t status;
    uint32_t sleep_ticks;       /* remaining SysTick ticks while TASK_SLEEPING */
    void *waiting_obj;          /* object this task is blocked on while TASK_BLOCKED, else NULL */
    uint8_t id;
    uint8_t priority;           /* effective priority: higher number = higher priority (idle = 0) */
    uint8_t base_priority;      /* priority given at creation (restored after priority inheritance) */
}   TCB_t;



void task_create(TCB_t *tcb, uint8_t id, uint8_t priority, void (*task_func)(void), uint32_t *stack, uint32_t stack_size);
void task_exit_handler(void);


#endif
