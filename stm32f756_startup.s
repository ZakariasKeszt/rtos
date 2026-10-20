.syntax unified
  .cpu cortex-m7
  .thumb

  /* Expose these to the linker */
  .global g_pfnVectors
  .global Default_Handler
  .global Reset_Handler

/* ====================================================================
 * Reset Handler: The first code executed after power-on
 * ==================================================================== */
  .section .text.Reset_Handler
  .type Reset_Handler, %function
Reset_Handler:
  /* 1. Set the stack pointer explicitly */
  ldr sp, =_estack

  /* 2. Copy the .data segment from Flash to RAM */
  ldr r0, =_la_data    /* Load address (in Flash) */
  ldr r1, =_sdata      /* Destination start (in RAM) */
  ldr r2, =_edata      /* Destination end (in RAM) */

copy_data_loop:
  cmp r1, r2           /* Are we at the end of .data? */
  ittt lt              /* If-Then block for Less-Than */
  ldrlt r3, [r0], #4   /* Load word from Flash, increment pointer */
  strlt r3, [r1], #4   /* Store word to RAM, increment pointer */
  blt copy_data_loop   /* Branch back if still less than */

  /* 3. Zero out the .bss segment in RAM */
  movs r0, #0          /* Zero value */
  ldr r1, =_sbss       /* Destination start (in RAM) */
  ldr r2, =_ebss       /* Destination end (in RAM) */

zero_bss_loop:
  cmp r1, r2           /* Are we at the end of .bss? */
  itt lt
  strlt r0, [r1], #4   /* Store zero to RAM, increment pointer */
  blt zero_bss_loop

  /* 4. Branch to main() */
  bl main

  /* 5. Trap the CPU if main() somehow returns */
trap:
  b trap

  .size Reset_Handler, .-Reset_Handler

/* ====================================================================
 * Default Handler: Catch-all for unhandled interrupts
 * ==================================================================== */
  .section .text.Default_Handler,"ax",%progbits
  .type Default_Handler, %function
Default_Handler:
  b Default_Handler
  .size Default_Handler, .-Default_Handler

/* ====================================================================
 * The Vector Table: Placed at 0x08000000 by the linker script
 * ==================================================================== */
  .section .isr_vector,"a",%progbits
  .type g_pfnVectors, %object
  .size g_pfnVectors, .-g_pfnVectors

g_pfnVectors:
  .word _estack             /* 0: Top of Stack */
  .word Reset_Handler       /* 1: Reset Handler */
  .word NMI_Handler         /* 2: NMI Handler */
  .word HardFault_Handler   /* 3: Hard Fault Handler */
  .word 0                   /* 4: Reserved */
  .word 0                   /* 5: Reserved */
  .word 0                   /* 6: Reserved */
  .word 0                   /* 7: Reserved */
  .word 0                   /* 8: Reserved */
  .word 0                   /* 9: Reserved */
  .word 0                   /* 10: Reserved */
  .word SVC_Handler         /* 11: SVCall Handler */
  .word 0                   /* 12: Reserved */
  .word 0                   /* 13: Reserved */
  .word PendSV_Handler      /* 14: PendSV Handler */
  .word SysTick_Handler     /* 15: SysTick Handler */

/* ====================================================================
 * Weak Aliases: Route unwritten handlers to the Default_Handler
 * ==================================================================== */
  .weak NMI_Handler
  .thumb_set NMI_Handler, Default_Handler

  .weak HardFault_Handler
  .thumb_set HardFault_Handler, Default_Handler

  .weak SVC_Handler
  .thumb_set SVC_Handler, Default_Handler

  .weak PendSV_Handler
  .thumb_set PendSV_Handler, Default_Handler

  .weak SysTick_Handler
  .thumb_set SysTick_Handler, Default_Handler
