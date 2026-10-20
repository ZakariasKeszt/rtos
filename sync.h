#ifndef _SYNC_H
#define _SYNC_H

#include <stdint.h>
#include "task.h"

/* Counting semaphore (a binary semaphore is simply max_count = 1). */
typedef struct {
    uint32_t count;         /* number of available "tokens" */
    uint32_t max_count;     /* count never goes above this */
} semaphore_t;

/* Non-recursive mutex with basic priority inheritance. */
typedef struct {
    uint8_t locked;         /* 1 while owned */
    TCB_t *owner;           /* task that holds the mutex, NULL when free */
} mutex_t;

void sem_init(semaphore_t *sem, uint32_t initial_count, uint32_t max_count);
void sem_wait(semaphore_t *sem);         /* blocks until a token is available */
int  sem_try_wait(semaphore_t *sem);     /* 0 = taken, -1 = count was 0 (never blocks) */
int  sem_give(semaphore_t *sem);         /* 0 = given, -1 = already at max_count; ISR-safe */

void mutex_init(mutex_t *mtx);
void mutex_lock(mutex_t *mtx);           /* blocks until the mutex is free (task context only) */
int  mutex_unlock(mutex_t *mtx);         /* 0 = unlocked, -1 = caller is not the owner */

#endif
