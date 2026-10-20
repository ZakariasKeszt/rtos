#include "queue.h"
#include "stm32f7xx.h"

/*
 * Classic "bounded buffer" built from two counting semaphores:
 *
 *   spaces = number of free slots   (starts at capacity)
 *   items  = number of filled slots (starts at 0)
 *
 *   send:    wait(spaces) -> write slot -> give(items)
 *   receive: wait(items)  -> read slot  -> give(spaces)
 *
 * Senders block on &q->spaces and receivers on &q->items, so each kind
 * of waiter has its own waiting_obj and the semaphore code does all of
 * the blocking/waking for us.
 */

void queue_init(queue_t *q, uint32_t *buffer, uint32_t capacity){
    q->buffer = buffer;
    q->capacity = capacity;
    q->head = 0;
    q->tail = 0;
    sem_init(&q->items, 0, capacity);
    sem_init(&q->spaces, capacity, capacity);
}

/* Copy one item into the ring buffer (a free slot is already reserved). */
static void queue_put(queue_t *q, uint32_t item){
    __disable_irq();
    q->buffer[q->tail] = item;
    q->tail = (q->tail + 1) % q->capacity;
    __enable_irq();
}

/* Copy one item out of the ring buffer (an item is already reserved). */
static void queue_get(queue_t *q, uint32_t *item){
    __disable_irq();
    *item = q->buffer[q->head];
    q->head = (q->head + 1) % q->capacity;
    __enable_irq();
}

void queue_send(queue_t *q, uint32_t item){
    sem_wait(&q->spaces);       /* block while the queue is full */
    queue_put(q, item);
    sem_give(&q->items);        /* wake a waiting receiver */
}

void queue_receive(queue_t *q, uint32_t *item){
    sem_wait(&q->items);        /* block while the queue is empty */
    queue_get(q, item);
    sem_give(&q->spaces);       /* wake a waiting sender */
}

int queue_try_send(queue_t *q, uint32_t item){
    if(sem_try_wait(&q->spaces) != 0){
        return -1;              /* full */
    }
    queue_put(q, item);
    sem_give(&q->items);
    return 0;
}

int queue_try_receive(queue_t *q, uint32_t *item){
    if(sem_try_wait(&q->items) != 0){
        return -1;              /* empty */
    }
    queue_get(q, item);
    sem_give(&q->spaces);
    return 0;
}
