#ifndef _QUEUE_H
#define _QUEUE_H

#include <stdint.h>
#include "sync.h"

/*
 * Fixed-size message queue of uint32_t items (ring buffer).
 * The caller provides the storage array, e.g.
 *
 *     static uint32_t buf[8];
 *     static queue_t  q;
 *     queue_init(&q, buf, 8);
 *
 * Bigger messages: send a pointer/index cast to uint32_t, or pack fields.
 */
typedef struct {
    uint32_t *buffer;       /* storage, 'capacity' items long */
    uint32_t capacity;
    uint32_t head;          /* index of the next item to read */
    uint32_t tail;          /* index of the next free slot to write */
    semaphore_t items;      /* counts filled slots -> receivers wait on this */
    semaphore_t spaces;     /* counts free slots   -> senders wait on this */
} queue_t;

void queue_init(queue_t *q, uint32_t *buffer, uint32_t capacity);

/* Blocking: wait until there is room / an item. Task context only. */
void queue_send(queue_t *q, uint32_t item);
void queue_receive(queue_t *q, uint32_t *item);

/* Non-blocking: return 0 on success, -1 if full / empty. Also usable from ISRs. */
int  queue_try_send(queue_t *q, uint32_t item);
int  queue_try_receive(queue_t *q, uint32_t *item);

#endif
