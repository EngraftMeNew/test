#include <asm.h>
#include <csr.h>


ENTRY(setup_exception)

  //异常入口函数地址写入t0->stvec
  /*  save exception_handler_entry into STVEC */
  la t0, exception_handler_entry
  csrw stvec, t0

  //全局中断使能
  /* enable interrupts globally */
  csrr t0,sstatus
  li t1,SR_SIE
  or t0,t0,t1
  csrw sstatus,t0
  ret
ENDPROC(setup_exception)
