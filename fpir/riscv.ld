OUTPUT_ARCH( "riscv" )
ENTRY( _entry )

SECTIONS
{
  PROVIDE(_memory_start = 0x80000000);
  /*
   * . = 0xA0000000;
   */
   . = 0x80000000;
  .text : {
    PROVIDE(_text_start = .);
    *(.text.entry)
    *(.text .text.*)
    . = ALIGN(0x1000);
    PROVIDE(_text_end = .);
  }

  .rodata : {
    PROVIDE(_rodata_start = .);
    *(.srodata .srodata.*)
    *(.rodata .rodata.*)
    . = ALIGN(0x1000);
    PROVIDE(_rodata_end = .);
  }
  .data : {
    PROVIDE(_data_start = .);
    *(.sdata .sdata.*)
    *(.data .data.*)
    . = ALIGN(0x1000);
    PROVIDE(_data_end = .);
  }
  .bss : {
    . = ALIGN(0x1000);
    PROVIDE(_bss_start = .);
    *(.sbss .sbss.*)
    *(.bss .bss.*)
    . = ALIGN(0x1000);
    PROVIDE(_bss_end = .);
  }
  /* lower guard page included in above */
  .stacks : {
    . = ALIGN(0x1000);
    PROVIDE(_stacks_start = .);
    . = . + 0x4000;
    . = ALIGN(0x1000);
    PROVIDE(_stacks_end = .);
  }
  .kernel : {
    PROVIDE(BM_TEXT = .);
    *(.kernel .kernel.*)
    . = ALIGN(0x1000);
  }
  .mainmem : {
    PROVIDE(MAINMEM = .);
  }

  PROVIDE(_memory_end = 0xA0000000 + 256M);
}
