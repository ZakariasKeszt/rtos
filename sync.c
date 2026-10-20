#include "sync.h"
#include "sched.h"
#include "stm32f7xx.h"
#include <stddef.h>

/*
 * Blocking pattern used everywhere in this file:
 *
 *   __disable_irq();
 *   while(resource not available){
 *       scheduler_block_current(obj);   // status = BLOCKED, waiting_obj = obj
 *       scheduler_reschedule();         // pick another task, pend PendSV
 *       __enable_irq();                 // PendSV runs HERE -> we are switched out
 *       __disable_irq();                // ...and continue here once woken up
 *   }
 *   take resource;
 *   __enable_irq();
 *
 * The 'while' (not 'if') matters: when we are woken, another task may have
 * taken the resource before we got the CPU, so we must check again.
 */

/* ======================== Semaphore ======================== */

void sem_init(semaphore_t *sem, uint32_t initial_count, uint32_t max_count){
    sem->count = initial_count;
    sem->max_count = max_count;
}

void sem_wait(semaphore_t *sem){
    __disable_irq();

    while(sem->count == 0){
        scheduler_block_current(sem);
        scheduler_reschedule();
        __enable_irq();
        __disable_irq();
    }
    sem->count--;

    __enable_irq();
}

int sem_try_wait(semaphore_t *sem){
    int result = -1;

    __disable_irq();
    if(sem->count > 0){
        sem->count--;
        result = 0;
    }
    __enable_irq();

    return result;
}

int sem_give(semaphore_t *sem){
    int result = -1;

    __disable_irq();
    if(sem->count < sem->max_count){
        sem->count++;
        scheduler_wake_one(sem);        /* highest-priority waiter becomes READY */
        scheduler_reschedule();         /* preempt us if it has higher priority */
        result = 0;
    }
    __enable_irq();

    return result;
}

/* ========================== Mutex ========================== */

void mutex_init(mutex_t *mtx){
    mtx->locked = 0;
    mtx->owner = NULL;
}

void mutex_lock(mutex_t *mtx){
    __disable_irq();

    while(mtx->locked){
        /*
         * Priority inheritance: if a lower-priority task holds the mutex,
         * temporarily raise it to our priority so a medium-priority task
         * cannot keep it off the CPU (priority inversion).
         */
        if(mtx->owner->priority < current_task->priority){
            mtx->owner->priority = current_task->priority;
        }

        scheduler_block_current(mtx);
        scheduler_reschedule();
        __enable_irq();
        __disable_irq();
    }
    mtx->locked = 1;
    mtx->owner = current_task;

    __enable_irq();
}

int mutex_unlock(mutex_t *mtx){
    __disable_irq();

    if(!mtx->locked || mtx->owner != current_task){
        __enable_irq();
        return -1;                      /* only the owner may unlock */
    }

    mtx->locked = 0;
    mtx->owner = NULL;

    /* Drop any inherited priority (simple model: one mutex held at a time). */
    current_task->priority = current_task->base_priority;

    scheduler_wake_one(mtx);
    scheduler_reschedule();

    __enable_irq();
    return 0;
}
