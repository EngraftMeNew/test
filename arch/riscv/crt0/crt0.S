#include <asm.h>

#define USER_STACK      0x52500000

.section ".entry_function","ax"
ENTRY(_start)

    /* setup C runtime: 建立栈帧，保存 ra/s0 */
    la      t0, __bss_start   
    la      t1, __BSS_END__    

clear:
    sw      zero, 0(t0)       
    add     t0, t0, 4
    bltu    t0, t1, clear



    jal    main


    /************************************************************/
    /* TODO: [P3] call sys_exit */
    /************************************************************/
    call sys_exit
// while(1) loop, unreachable here
loop:
    wfi
    j       loop

END(_start)
