#include "task.h"
#include "stm32f7xx.h"
#include <stddef.h>


void task_create(TCB_t *tcb, uint8_t id, uint8_t priority, void (*task_func)(void), uint32_t *stack, uint32_t stack_size){
    uint32_t *sp = stack + stack_size;
    // hardware-stacked frame
    *(--sp) = 0x01000000;                  // xPSR: Thumb bit
    *(--sp) = (uint32_t)task_func;         // PC
    *(--sp) = (uint32_t)task_exit_handler; // LR: where the task "returns" to if task_func ever exits
    *(--sp) = 0x12121212;                  // R12
    *(--sp) = 0x03030303;                  // R3
    *(--sp) = 0x02020202;                  // R2
    *(--sp) = 0x01010101;                  // R1
    *(--sp) = 0x00000000;                  // R0

    // software-stacked frame
    *(--sp) = 0x11111111;                  // R11
    *(--sp) = 0x10101010;                  // R10
    *(--sp) = 0x09090909;                  // R9
    *(--sp) = 0x08080808;                  // R8
    *(--sp) = 0x07070707;                  // R7
    *(--sp) = 0x06060606;                  // R6
    *(--sp) = 0x05050505;                  // R5
    *(--sp) = 0x04040404;                  // R4

    tcb->sp = sp;
    tcb->status = TASK_READY;
    tcb->sleep_ticks = 0;
    tcb->waiting_obj = NULL;
    tcb->id = id;
    tcb->priority = priority;
    tcb->base_priority = priority;
    tcb->next = NULL;
}

void task_exit_handler(void){
    __disable_irq();
    while(1) { __NOP(); }
}
