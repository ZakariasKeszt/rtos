#include "task.h"
#include "stm32f7xx.h"
#include "sched.h"
#include <stddef.h>



void port_yield(){
    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
}

void port_start_first_task(TCB_t *first){
    next_task = first;
/*
    __set_PSP((uint32_t)first->sp + 8 * 4);*/
    __set_CONTROL(0x00);
    __ISB();
    port_yield();

    __enable_irq();
    while(1) {}
}

void SysTick_Handler(void){
    if(current_task == NULL) return;    /* scheduler not started yet */

    scheduler_tick();                   /* wake tasks whose delay expired */
    schedule();                         /* pick highest-priority READY task */
    if(next_task != current_task){
        port_yield();                   /* context switch only if needed */
    }
}
