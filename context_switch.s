    .syntax unified
    .cpu cortex-m7
    .thumb

    .global PendSV_Handler
    .extern current_task
    .extern next_task
    .thumb_func
PendSV_Handler:


    LDR R0, =current_task
    LDR R0, [R0]
    CBZ R0, restore

    MRS R1, PSP
    STMDB R1!, {R4-R11}
    STR R1, [R0]

restore:
    LDR R0, =next_task
    LDR R0, [R0]
    LDR R1, [R0]
    LDMIA R1!, {R4-R11}
    MSR PSP, R1

    LDR R0, =current_task
    LDR R2, =next_task
    LDR R2, [R2]
    STR R2, [R0]

    LDR LR, =0XFFFFFFFD
    BX LR
