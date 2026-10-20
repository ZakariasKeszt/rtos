/* The entry point of the application */
ENTRY(Reset_Handler)

/* Define the physical memory mapped in your Renode .repl */
MEMORY
{
  FLASH (rx)      : ORIGIN = 0x08000000, LENGTH = 1024K
  RAM (xrw)       : ORIGIN = 0x20000000, LENGTH = 320K
}

/* Stack Pointer RAM elejére */
_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
  /* Vektor tabla */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH

  /* Programkód és read-only cuccok */
  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text.*)
    *(.rodata)
    *(.rodata.*)
    . = ALIGN(4);
  } >FLASH

  /* * .data is initialized variables.
   * It lives in FLASH but needs to be copied to RAM at startup.
   * _la_data holds the load address (in Flash).
   */
  _la_data = LOADADDR(.data);

  .data :
  {
    . = ALIGN(4);
    _sdata = .;        /* Start of .data in RAM */
    *(.data)
    *(.data.*)
    . = ALIGN(4);
    _edata = .;        /* End of .data in RAM */
  } >RAM AT> FLASH

  /* .bss is uninitialized variables (defaults to 0) */
  .bss :
  {
    . = ALIGN(4);
    _sbss = .;         /* Start of .bss in RAM */
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss.*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;         /* End of .bss in RAM */
    __bss_end__ = _ebss;
  } >RAM
}
