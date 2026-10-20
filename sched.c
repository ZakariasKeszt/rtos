#include "sched.h"
#include "port.h"
#include "task.h"
#include <stdint.h>
#include <stddef.h>
#include "stm32f7xx.h"

static TCB_t idle_tcb;
/* 8-byte aligned: the Cortex-M ABI (AAPCS) requires an 8-byte aligned stack */
static uint32_t idle_stack[IDLE_STACK_SIZE] __attribute__((aligned(8)));

TCB_t *current_task = NULL;
TCB_t *next_task = NULL;

void scheduler_idle(){
    while(1) { __WFI(); }
}

void scheduler_init(){
    /* Keep interrupts off until the first task starts
       (port_start_first_task() re-enables them). */
    __disable_irq();

    task_create(&idle_tcb, 0, 0, scheduler_idle, idle_stack, IDLE_STACK_SIZE);
    idle_tcb.next = &idle_tcb;
}

void scheduler_start(){
    schedule();                 /* pick the highest-priority READY task */
    port_start_first_task(next_task);
}

void scheduler_add_task(TCB_t *tcb, uint8_t id, uint8_t priority, void (*task_func)(void), uint32_t *stack, uint32_t stack_size){
    task_create(tcb, id, priority, task_func, stack, stack_size);
    tcb->next = idle_tcb.next;
    idle_tcb.next = tcb;
    tcb->status = TASK_READY;
}

/*
 * Preemptive priority scheduling with round-robin tie-breaking.
 *
 * Walk the circular list exactly once, starting at current_task->next, and
 * remember the READY task with the highest priority. A candidate only
 * replaces the best one if its priority is strictly greater, so among tasks
 * with equal priority the one found FIRST after current_task wins. The
 * current task itself is visited last, so it only keeps the CPU if no other
 * task of the same priority is ready -> equal priorities take turns.
 *
 * The idle task is skipped in the search and only chosen if nothing else
 * is READY.
 *
 * Note: the running task keeps status TASK_READY (READY = "runnable").
 */
void schedule(){
    /* Before the first task runs current_task is NULL: start from the list head. */
    TCB_t *start = (current_task != NULL) ? current_task->next : idle_tcb.next;
    TCB_t *candidate = start;
    TCB_t *best = NULL;

    do {
        if(candidate != &idle_tcb && candidate->status == TASK_READY){
            if(best == NULL || candidate->priority > best->priority){
                best = candidate;
            }
        }
        candidate = candidate->next;
    } while(candidate != start);

    next_task = (best != NULL) ? best : &idle_tcb;
}

/*
 * Called from SysTick_Handler once per tick.
 * Counts down every sleeping task and makes it READY when its delay expires.
 */
void scheduler_tick(void){
    TCB_t *t = &idle_tcb;

    do {
        if(t->status == TASK_SLEEPING){
            if(t->sleep_ticks > 0){
                t->sleep_ticks--;
            }
            if(t->sleep_ticks == 0){
                t->status = TASK_READY;
            }
        }
        t = t->next;
    } while(t != &idle_tcb);
}

/*
 * Put the calling task to sleep for 'ticks' SysTick periods.
 * task_delay(0) simply yields to another ready task of equal/higher priority.
 */
void task_delay(uint32_t ticks){
    __disable_irq();

    if(ticks > 0){
        current_task->sleep_ticks = ticks;
        current_task->status = TASK_SLEEPING;
    }
    schedule();
    port_yield();               /* PendSV is pended now, runs once IRQs are enabled */

    __enable_irq();
}

/* ------------------------------------------------------------------------
 * Blocking helpers shared by semaphores, mutexes and queues.
 * All of them must be called with interrupts disabled.
 * ------------------------------------------------------------------------ */

/* Mark the running task as blocked on 'obj' (a semaphore, mutex, ...). */
void scheduler_block_current(void *obj){
    current_task->status = TASK_BLOCKED;
    current_task->waiting_obj = obj;
}

/*
 * Find the highest-priority task blocked on 'obj' and make it READY.
 * Returns the woken task, or NULL if nobody was waiting.
 */
TCB_t *scheduler_wake_one(void *obj){
    TCB_t *t = &idle_tcb;
    TCB_t *best = NULL;

    do {
        if(t->status == TASK_BLOCKED && t->waiting_obj == obj){
            if(best == NULL || t->priority > best->priority){
                best = t;
            }
        }
        t = t->next;
    } while(t != &idle_tcb);

    if(best != NULL){
        best->status = TASK_READY;
        best->waiting_obj = NULL;
    }
    return best;
}

/* Re-run the scheduler and request a context switch if another task should run. */
void scheduler_reschedule(void){
    if(current_task == NULL) return;    /* scheduler not started yet */

    schedule();
    if(next_task != current_task){
        port_yield();
    }
}
