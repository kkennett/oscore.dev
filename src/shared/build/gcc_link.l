/*
 *    Copyright <C> 2017, Kurt Kennett (K2)
 *
 *    All rights reserved.
 */
SECTIONS { 
  . = 0x10000000;
  .text : { *(.text) *(.text.*) }
  . = ALIGN(4096);
  .rodata : { *(.rodata) *(.rodata.*) *(.eh_frame) }
  __ctors = .;
  .ctors : { *(.ctors) *(.init_array) *(.init_array.*) }
  . = ALIGN(4096);
  .data : { *(.data) *(.data.*) }
  .bss :  { *(.bss) *(.bss.*) }
  __data_end = .;
}
__ctors_count = SIZEOF(.ctors) / 4;
