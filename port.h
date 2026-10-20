#ifndef _PORT_H
#define _PORT_H

#include "task.h"

extern TCB_t *current_task;
extern TCB_t *next_task;

void port_yield(void);
void port_start_first_task(TCB_t *first);

#endif
