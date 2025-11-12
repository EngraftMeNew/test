/*为C语言运行幻剑创建最小启动框架，建立栈，清零bss段，调用main*/
/*位于boot->kernel->用户任务的最初入口*/
#include <asm.h>

.section ".entry_function","ax"
ENTRY(_start)

    /* setup C runtime: 建立栈帧，保存 ra/s0 */
    addi    t2,sp,0
    la      t0, __bss_start   
    la      t1, __BSS_END__    

clear:
    sw      zero, 0(t0)       
    add     t0, t0, 4
    bltu    t0, t1, clear

    addi sp, sp, -16
    sd ra, 0(sp)
    sd t2, 8(sp)

    jal    main

    ld      ra, 0(sp)
    ld      t2, 8(sp)
    addi    sp, sp, 16
    addi    sp,t2,0
    jalr      ra

    /************************************************************/
    /* Do not touch this comment. Reserved for future projects. */
    /************************************************************/
// while(1) loop, unreachable here
loop:
    wfi
    j       loop

END(_start)
