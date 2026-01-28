ENTRY(_start)
SECTIONS
{
    . = 0x400000; /* Standard Load Address for executables */
    
    .text : {
        *(.text)
    }
    
    .data : {
        *(.data)
    }
    
    .rodata : {
        *(.rodata)
    }
    
    .bss : {
        *(.bss)
    }
}