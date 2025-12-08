#include <common.h>
#include <asm.h>
#include <asm/unistd.h>
#include <os/loader.h>
#include <os/irq.h>
#include <os/sched.h>
#include <os/lock.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <os/mm.h>
#include <os/time.h>
#include <sys/syscall.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <type.h>
#include <csr.h>
#include <os/smp.h>

extern void ret_from_exception();

// Task info array
task_info_t tasks[TASK_MAXNUM];
int task_num = 0;
static void init_jmptab(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR] = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ] = (long (*)())sd_read;
    jmptab[SD_WRITE] = (long (*)())sd_write;
    jmptab[QEMU_LOGGING] = (long (*)())qemu_logging;
    jmptab[SET_TIMER] = (long (*)())set_timer;
    jmptab[READ_FDT] = (long (*)())read_fdt;
    jmptab[PRINT] = (long (*)())printk;
    jmptab[YIELD] = (long (*)())do_scheduler;
    jmptab[MUTEX_INIT] = (long (*)())do_mutex_lock_init;
    jmptab[MUTEX_ACQ] = (long (*)())do_mutex_lock_acquire;
    jmptab[MUTEX_RELEASE] = (long (*)())do_mutex_lock_release;

    //  (S-core) initialize system call table.
    jmptab[MOVE_CURSOR] = (long (*)())screen_move_cursor;
    jmptab[WRITE] = (long (*)())screen_write;
    jmptab[REFLUSH] = (long (*)())screen_reflush;
}

void init_task_info(int app_info_loc, int app_info_size)
{
    // Init 'tasks' array via reading app-info sector
    // NOTE: You need to get some related arguments from bootblock first
    int start_sec, blocknums;
    start_sec = app_info_loc / SECTOR_SIZE;
    blocknums = NBYTES2SEC(app_info_loc + app_info_size) - start_sec;
    int task_info_addr = TASK_INFO_MEM;
    bios_sd_read(task_info_addr, blocknums, start_sec);
    int start_addr = (TASK_INFO_MEM + app_info_loc - start_sec * SECTOR_SIZE);
    uint8_t *tmp = (uint8_t *)(start_addr);
    memcpy((uint8_t *)tasks, tmp, app_info_size);
}

/************************************************************/
void init_pcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
    pcb_t *pcb, int argc, char *argv[])
{
    /*  initialization of registers on kernel stack
     * HINT: sp, ra, sepc, sstatus
     * NOTE: To run the task in user mode, you should set corresponding bits
     *     of sstatus(SPP, SPIE, etc.).
     */
    regs_context_t *pt_regs =
        (regs_context_t *)(kernel_stack - sizeof(regs_context_t));
    pt_regs->regs[1] = (uint64_t)entry_point; // ra
    pt_regs->regs[2] = user_stack;            // sp
    pt_regs->regs[4] = (uint64_t)pcb;         // tp
    pt_regs->sstatus = SR_SPIE;               // SPIE set to 1
    pt_regs->sepc = (uint64_t)entry_point;
    pt_regs->regs[10] = (reg_t)argc; // a0 = argc
    pt_regs->regs[11] = (reg_t)argv; // a1 = argv

    /* set sp to simulate just returning from switch_to
     * NOTE: you should prepare a stack, and push some values to
     * simulate a callee-saved context.
     */
    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));
    pcb->kernel_sp = kernel_stack - sizeof(switchto_context_t) - sizeof(regs_context_t);
    pt_switchto->regs[0] = (uint64_t)ret_from_exception; // ra
    pt_switchto->regs[1] = pcb->kernel_sp;               // sp
}

void init_pcb(void)
{
    /*  load needed tasks and init their corresponding PCB */

    uint64_t entry_addr;

    pid0_pcb.status = TASK_RUNNING;
    pid0_pcb.list.prev = NULL;
    pid0_pcb.list.next = NULL;
    s_pid0_pcb.status = TASK_READY;
    s_pid0_pcb.list.prev = NULL;
    s_pid0_pcb.list.next = NULL;

    // load task by name;
    for (int i = 0; i < NUM_MAX_TASK; i++)
    {
        pcb[i].status = TASK_EXITED;
    }

    /* remember to initialize 'current_running' */
    current_running[0] = &pid0_pcb;
    current_running[1] = &s_pid0_pcb;
}

static void init_syscall(void)
{
    // initialize system call table.
    syscall[SYSCALL_SLEEP] = (long (*)())do_sleep;
    syscall[SYSCALL_YIELD] = (long (*)())do_scheduler;
    syscall[SYSCALL_WRITE] = (long (*)())screen_write;
    syscall[SYSCALL_CURSOR] = (long (*)())screen_move_cursor;
    syscall[SYSCALL_REFLUSH] = (long (*)())screen_reflush;
    syscall[SYSCALL_GET_TIMEBASE] = (long (*)())get_time_base;
    syscall[SYSCALL_GET_TICK] = (long (*)())get_ticks;
    syscall[SYSCALL_LOCK_INIT] = (long (*)())do_mutex_lock_init;
    syscall[SYSCALL_LOCK_ACQ] = (long (*)())do_mutex_lock_acquire;
    syscall[SYSCALL_LOCK_RELEASE] = (long (*)())do_mutex_lock_release;
    syscall[SYSCALL_PS] = (long (*)())do_process_show;
    syscall[SYSCALL_CLEAR] = (long (*)())screen_clear;
    syscall[SYSCALL_EXEC] = (long (*)())do_exec;
    syscall[SYSCALL_WAITPID] = (long (*)())do_waitpid;
    syscall[SYSCALL_EXIT] = (long (*)())do_exit;
    syscall[SYSCALL_KILL] = (long (*)())do_kill;
    syscall[SYSCALL_GETPID] = (long (*)())do_getpid;
    syscall[SYSCALL_READCH] = (long (*)())bios_getchar;
    syscall[SYSCALL_WRITECH] = (long (*)())screen_write_ch;
    /*barrier*/
    syscall[SYSCALL_BARR_INIT] = (long (*)())do_barrier_init;
    syscall[SYSCALL_BARR_WAIT] = (long (*)())do_barrier_wait;
    syscall[SYSCALL_BARR_DESTROY] = (long (*)())do_barrier_destroy;

    /*condition*/
    syscall[SYSCALL_COND_INIT] = (long (*)())do_condition_init;
    syscall[SYSCALL_COND_WAIT] = (long (*)())do_condition_wait;
    syscall[SYSCALL_COND_SIGNAL] = (long (*)())do_condition_signal;
    syscall[SYSCALL_COND_BROADCAST] = (long (*)())do_condition_broadcast;
    syscall[SYSCALL_COND_DESTROY] = (long (*)())do_condition_destroy;

    /*mailbox*/
    syscall[SYSCALL_MBOX_SEND] = (long (*)())do_mbox_send;
    syscall[SYSCALL_MBOX_RECV] = (long (*)())do_mbox_recv;
    syscall[SYSCALL_MBOX_OPEN] = (long (*)())do_mbox_open;
    syscall[SYSCALL_MBOX_CLOSE] = (long (*)())do_mbox_close;
}
/************************************************************/

/*
 * Once a CPU core calls this function,
 * it will stop executing!
 */
static void kernel_brake(void)
{
    disable_interrupt();
    while (1)
        __asm__ volatile("wfi");
}

int main(int app_info_loc, int app_info_size)
{
    int tmp_cpu_id = get_current_cpu_id();

    if (tmp_cpu_id == 0)
    {
        // ------------ 只在 CPU0 上做的一次性初始化 ------------

        // SMP 初始化 + 大内核锁
        smp_init();
        lock_kernel();

        // Init jump table provided by kernel and bios(ΦωΦ)
        init_jmptab();

        // Init task information (〃'▽'〃)
        init_task_info(app_info_loc, app_info_size);

        // Init Process Control Blocks |•'-'•) ✧
        init_pcb();
        printk("> [INIT] PCB initialization succeeded.\n");

        // Read CPU frequency (｡•ᴗ-)_
        time_base = bios_read_fdt(TIMEBASE);

        // Init lock mechanism o(´^｀)o
        init_locks();
        printk("> [INIT] Lock mechanism initialization succeeded.\n");

        /*
         * Just start kernel with VM and print this string
         * in the first part of task 1 of project 4.
         * NOTE: if you use SMP, then every CPU core should call
         *  `kernel_brake()` to stop executing!
         */
        printk("> [INIT] CPU #%u has entered kernel with VM!\n",
               (unsigned int)get_current_cpu_id());
        // TODO: [p4-task1 cont.] remove the brake and continue to start user processes.
        kernel_brake();

        // TODO: [p2-task4] Setup timer interrupt and enable all interrupt globally
        // NOTE: The function of sstatus.sie is different from sie's

        // Init interrupt (^_^)
        init_exception();
        printk("> [INIT] Interrupt processing initialization succeeded.\n");

        // init barriers
        init_barriers();
        printk("> [INIT] Barrier initialization succeeded.\n");

        init_conditions();
        printk("> [INIT] Condition initialization succeeded.\n");

        init_mbox();
        printk("> [INIT] Mailbox initialization succeeded.\n");

        // Init system call table (0_0)
        init_syscall();
        printk("> [INIT] System call initialized successfully.\n");

        // Init screen (QAQ)
        init_screen();
        printk("> [INIT] SCREEN initialization succeeded.\n");

        // 在 CPU0 上起 shell
        do_exec("shell", 0, NULL);

        // 初始化完，放开锁，叫醒其他 hart
        unlock_kernel();
        wakeup_other_hart(NULL);

        // 再抢回锁，从此进入正常调度
        lock_kernel();
        cpu_id = 0;
    }
    else
    {
        // ------------ CPU1 路径（不再做各种 init_*） ------------

        lock_kernel(); // 等 CPU0 做完初始化
        cpu_id = 1;    // 强制置为 1，避免用 mhartid 乱飞
        current_running[cpu_id]->status = TASK_RUNNING;
    }

    // 这里开始两个核执行相同的代码

    setup_exception(); // 设置 stvec、SIE 等异常入口

    // 每个核设置自己的 timer
    bios_set_timer(get_ticks() + TIMER_INTERVAL);

    if (cpu_id == 0)
        printk("> [INIT] CPU 0 initialization succeeded.\n");
    else
        printk("> [INIT] CPU 1 initialization succeeded.\n");

    unlock_kernel();

    while (1)
    {
        enable_preempt();
        asm volatile("wfi");
    }

    return 0;
}
