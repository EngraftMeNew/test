#include <os/irq.h>
#include <os/time.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/kernel.h>
#include <printk.h>
#include <assert.h>
#include <screen.h>

#define SCAUSE_IRQ_MASK 0x8000000000000000
handler_t irq_table[IRQC_COUNT];
handler_t exc_table[EXCC_COUNT];

// 统一的trap分发
// 根据scause判定中断/异常，再去表里调用具体函数
void interrupt_helper(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // TODO: [p2-task3] & [p2-task4] interrupt handler.
    // call corresponding handler by the value of `scause`
    // 高位=1表示中断；其余位为中断/异常编码
    uint64_t is_irq = (scause & SCAUSE_IRQ_MASK) != 0;
    uint64_t code = (scause & ~SCAUSE_IRQ_MASK); // 最高位清零作为下标

    if (is_irq)
    {
        if (code < IRQC_COUNT && irq_table[code])
            irq_table[code](regs, stval, scause);
        else
            handle_other(regs, stval, scause); 
    }
    else
    {
        if (code < EXCC_COUNT && exc_table[code])
            exc_table[code](regs, stval, scause);
        else
            handle_other(regs, stval, scause);
    }
}

void handle_irq_timer(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // TODO: [p2-task4] clock interrupt handler.
    // Note: use bios_set_timer to reset the timer and remember to reschedule
    uint64_t next = get_ticks() + TIMER_INTERVAL;
    bios_set_timer(next); 
    do_scheduler();       
}

void init_exception(void)
{
    /* TODO: [p2-task3] initialize exc_table */
    /* NOTE: handle_syscall, handle_other, etc.*/
    for (int i = 0; i < EXCC_COUNT; ++i)
        exc_table[i] = handle_other;
    exc_table[EXCC_SYSCALL] = handle_syscall;

    /* TODO: [p2-task4] initialize irq_table */
    /* NOTE: handle_int, handle_other, etc.*/
    for (int i = 0; i < IRQC_COUNT; ++i)
        irq_table[i] = handle_other;
    irq_table[IRQC_S_TIMER] = handle_irq_timer;

    /* TODO: [p2-task3] set up the entrypoint of exceptions */
    setup_exception();
}

// 兜底处理，打印相关信息
void handle_other(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    static const char *reg_name[] = {
        "zero ", " ra  ", " sp  ", " gp  ", " tp  ",
        " t0  ", " t1  ", " t2  ", "s0/fp", " s1  ",
        " a0  ", " a1  ", " a2  ", " a3  ", " a4  ",
        " a5  ", " a6  ", " a7  ", " s2  ", " s3  ",
        " s4  ", " s5  ", " s6  ", " s7  ", " s8  ",
        " s9  ", " s10 ", " s11 ", " t3  ", " t4  ",
        " t5  ", " t6  "};
    for (int i = 0; i < 32; i += 3)
    {
        for (int j = 0; j < 3 && i + j < 32; ++j)
        {
            printk("%s : %016lx ", reg_name[i + j], regs->regs[i + j]);
        }
        printk("\n\r");
    }
    printk("sstatus: 0x%lx sbadaddr: 0x%lx scause: %lu\n\r",
           regs->sstatus, regs->sbadaddr, regs->scause);
    printk("sepc: 0x%lx\n\r", regs->sepc);
    printk("tval: 0x%lx cause: 0x%lx\n", stval, scause);
    assert(0);
}
