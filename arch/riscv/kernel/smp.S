#include <asm.h>
#include <csr.h>

ENTRY(get_current_cpu_id)
  //csrr a0, CSR_MHARTID
  li a0,0
  jr ra
ENDPROC(get_current_cpu_id)
