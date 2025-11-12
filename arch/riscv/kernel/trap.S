#include <asm.h>
#include <csr.h>


ENTRY(setup_exception)

  //异常入口函数地址写入t0->stvec
  /* TODO: [p2-task3] save exception_handler_entry into STVEC */
  la t0, exception_handler_entry
  csrw stvec, t0

  //全局中断使能
  /* TODO: [p2-task4] enable interrupts globally */
  addi    sp, sp, -8   
  sd      ra, 0(sp)
  call    enable_interrupt   //设置sstates.SIE=1
  ld      ra, 0(sp)
  addi    sp, sp, 8
  ret
ENDPROC(setup_exception)
