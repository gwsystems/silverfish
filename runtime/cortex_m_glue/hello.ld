ENTRY(_start)
MEMORY{ ram : ORIGIN = 0x00010000, LENGTH = 0x1000 } SECTIONS {
    .text : { *(.text*)} > ram.rodata : { *(.rodata*)} > ram.bss : { *(.bss*)} > ram
}
