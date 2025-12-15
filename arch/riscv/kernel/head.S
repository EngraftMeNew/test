#include <asm.h>
#include <csr.h>

#define KERNEL_STACK		0xffffffc050500000
#define S_KERNEL_STACK  0xffffffc050600000
.section ".entry_function","ax"
ENTRY(_start)
  /* Mask all interrupts */
  csrw CSR_SIE, zero
  csrw CSR_SIP, zero

  //csrr t0, CSR_MHARTID  //读当前的cpu_id
  //bnez t0, s_start     //从核

  /*  clear BSS for flat non-ELF images */
  la t0, __bss_start
  la t1, __BSS_END__
do_clear:
  sw zero, 0(t0)
  addi t0, t0, 4
  bltu t0, t1, do_clear

  /*  setup C environment */

  la tp, pid0_pcb
  li sp, KERNEL_STACK
  j main

s_start:
  la tp, s_pid0_pcb
  li sp, S_KERNEL_STACK
  call main

loop:
  wfi
  j loop

END(_start)
