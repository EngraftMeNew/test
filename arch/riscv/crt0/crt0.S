#include <asm.h>

#define USER_STACK 0x52000000

.section ".entry_function","ax"
ENTRY(_start)

    /* TODO: [p1-task3] setup C runtime environment for the user program */
    la      t0, __bss_start     
    la      t1, __BSS_END__     

clear:
    sw    zero,0(t0)
    addi  t0,t0,4
    bltu  t0,t1,clear

setup_C:
    li  t0,USER_STACK
    sd  sp,0(t0)
    mv  sp,t0
    addi    sp,sp,-8
    sd  ra,0(sp)

    jal main
    /* TODO: [p1-task3] enter main function */


    /* TODO: [p1-task3] finish task and return to the kernel, replace this in p3-task2! */
    ld  ra,0(sp)
    ld  sp,8(sp)
    ret

    /************************************************************/
	/* Do not touch this comment. Reserved for future projects. */
	/************************************************************/
// while(1) loop, unreachable here
loop:
    wfi
    j loop

END(_start)
