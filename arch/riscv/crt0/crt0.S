#include <asm.h>

.section ".entry_function","ax"
ENTRY(_start)

    /* setup C runtime: 建立栈帧，保存 ra/s0 */
    addi    sp, sp, -16
    sd      s0, 8(sp)          
    sd      ra, 0(sp)         
    addi    s0, sp, 16         


    la      t0, __bss_start   
    la      t1, __BSS_END__    

clear:
    sw      zero, 0(t0)       
    addi    t0, t0, 4
    blt     t0, t1, clear

    call    main

    ld      ra, 0(sp)
    ld      s0, 8(sp)
    addi    sp, sp, 16
    jr      ra

    /************************************************************/
    /* Do not touch this comment. Reserved for future projects. */
    /************************************************************/
# while(1) loop, unreachable here
loop:
    wfi
    j       loop

END(_start)
