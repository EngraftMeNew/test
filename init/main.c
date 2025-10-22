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
#include <os/loader.h>
#include <type.h>
#include <csr.h>

#define VERSION_BUF 50
#define SECTOR_SIZE 512

// 共享地址
#define BATCH_MAILBOX_ADDR ((volatile int *)0x50510000)

#define BOOT_LOADER_SIG_OFFSET 0x1fe
#define OS_SIZE_LOC 0x1fc      /* 0x1fc: uint16_t, kernel bytes */
#define APPINFO_SIZE_LOC 0x1f8 /* 0x1f8: uint32_t, appinfo offset */
#define TASKNUM_LOC 0x1f6      /* 0x1f6: uint16_t, task count */
#define BATCH_OFFSET_LOC 0x1f0 /* 0x1f0: uint32_t, batch area offset */

#define BATCH_AREA_SIZE 512
#define NBYTES2SEC(n) ((uint32_t)((n) / SECTOR_SIZE) + (((n) % SECTOR_SIZE) ? 1u : 0u))

int version = 2; // version must between 0 and 9
char buf[VERSION_BUF];

extern void ret_from_exception();

// Task info array
task_info_t tasks[TASK_MAXNUM];

static void bytes_copy(void *dst, const void *src, unsigned long n)
{
    unsigned long i;
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    for (i = 0; i < n; ++i)
        d[i] = s[i];
}
static void bytes_set(void *dst, unsigned char v, unsigned long n)
{
    unsigned long i;
    unsigned char *d = (unsigned char *)dst;
    for (i = 0; i < n; ++i)
        d[i] = v;
}

static int bss_check(void)
{
    int i;
    for (i = 0; i < VERSION_BUF; ++i)
        if (buf[i] != 0)
            return 0;
    return 1;
}

static void init_jmptab(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;
    jmptab[CONSOLE_PUTSTR] = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
    jmptab[SD_WRITE]        = (long (*)())sd_write;
    jmptab[QEMU_LOGGING]    = (long (*)())qemu_logging;
    jmptab[SET_TIMER]       = (long (*)())set_timer;
    jmptab[READ_FDT]        = (long (*)())read_fdt;
    jmptab[MOVE_CURSOR]     = (long (*)())screen_move_cursor;
    jmptab[PRINT]           = (long (*)())printk;
    jmptab[YIELD]           = (long (*)())do_scheduler;
    jmptab[MUTEX_INIT]      = (long (*)())do_mutex_lock_init;
    jmptab[MUTEX_ACQ]       = (long (*)())do_mutex_lock_acquire;
    jmptab[MUTEX_RELEASE]   = (long (*)())do_mutex_lock_release;

    // TODO: [p2-task1] (S-core) initialize system call table.

}

/* 从 boot 扇区读取 appinfo_off 和 tasknum，并据此读出 task_info_t 表 */
static void init_task_info(void)
{
    // TODO: [p1-task4] Init 'tasks' array via reading app-info sector
    // NOTE: You need to get some related arguments from bootblock first
    uint8_t bootsec[SECTOR_SIZE];
    if (sd_read((unsigned)(uintptr_t)bootsec, 1, 0) < 0)
    {
        bios_putstr("sd_read boot sector failed\n\r");
        return;
    }

    /* 小端读取固定字段 */
    uint16_t os_size_le = 0, tasknum_le = 0;
    uint32_t appinfo_off_le = 0;

    bytes_copy(&os_size_le, bootsec + OS_SIZE_LOC, (unsigned long)sizeof(uint16_t));
    bytes_copy(&tasknum_le, bootsec + TASKNUM_LOC, (unsigned long)sizeof(uint16_t));
    bytes_copy(&appinfo_off_le, bootsec + APPINFO_SIZE_LOC, (unsigned long)sizeof(uint32_t));

    (void)os_size_le; /* 调试 */

    int tnum = (int)tasknum_le;
    if (tnum <= 0)
        return;
    if (tnum > TASK_MAXNUM)
        tnum = TASK_MAXNUM;

    unsigned appinfo_off = (unsigned)appinfo_off_le;
    unsigned read_bytes = (unsigned)(tnum * (int)sizeof(task_info_t));

    unsigned head_offset = appinfo_off % SECTOR_SIZE;
    unsigned start_lba = appinfo_off / SECTOR_SIZE;
    unsigned total_bytes = head_offset + read_bytes;
    unsigned nsec = NBYTES2SEC(total_bytes);

    uint8_t tmpbuf[2 * SECTOR_SIZE + TASK_MAXNUM * sizeof(task_info_t)];
    if (sd_read((unsigned)(uintptr_t)tmpbuf, nsec, start_lba) < 0)
    {
        bios_putstr("sd_read app-info failed\n\r");
        return;
    }
    bytes_copy((void *)tasks, tmpbuf + head_offset, (unsigned long)read_bytes);
}

/************************************************************/
static void init_pcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
    pcb_t *pcb)
{
     /* TODO: [p2-task3] initialization of registers on kernel stack
      * HINT: sp, ra, sepc, sstatus
      * NOTE: To run the task in user mode, you should set corresponding bits
      *     of sstatus(SPP, SPIE, etc.).
      */
    regs_context_t *pt_regs =
        (regs_context_t *)(kernel_stack - sizeof(regs_context_t));


    /* TODO: [p2-task1] set sp to simulate just returning from switch_to
     * NOTE: you should prepare a stack, and push some values to
     * simulate a callee-saved context.
     */
    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));

}

static void init_pcb(void)
{
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */


    /* TODO: [p2-task1] remember to initialize 'current_running' */

}

static void init_syscall(void)
{
    // TODO: [p2-task3] initialize system call table.
}
/************************************************************/

/* 查询任务名是否存在（按 tasks 表精确匹配） */
static int task_exists(const char *name)
{
    int i;
    for (i = 0; i < TASK_MAXNUM; ++i)
    {
        if (tasks[i].name[0] == '\0')
            break;
        if (!strcmp(tasks[i].name, name))
            return 1;
    }
    return 0;
}

/* 读取一行到 buf，最多 cap-1 字符；回显；返回长度 */
static int read_line_echo(char *buf, int cap)
{
    int len = 0;
    if (cap <= 0)
        return 0;

    for (;;)
    {
        char ch = port_read_ch();

        if (ch == '\r' || ch == '\n')
        {
            /* 行结束 */
            buf[len] = '\0';
            bios_putstr("\n\r");
            return len;
        }

        if ((ch == '\b' || (unsigned char)ch == 0x7F))
        {
            /* Backspace 或 Delete */
            if (len > 0)
            {
                --len;
                port_write_ch('\b');
                port_write_ch(' ');
                port_write_ch('\b');
            }
            continue;
        }

        /* 只接受可打印 ASCII，且保留 1 字节给 '\0' */
        if (ch >= ' ' && ch <= '~' && len < cap - 1)
        {
            buf[len++] = ch;
            port_write_ch(ch);
        }
        /* 其他字符直接忽略 */
    }
}

/* 跳过空白 */
static char *skip_ws(char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        ++p;
    return p;
}

/* 取下一个 token：从 p 复制到 name[]，最长 name_cap-1；返回下一个位置 */
static char *next_token(char *p, char *name, int name_cap)
{
    int k = 0;
    p = skip_ws(p);
    while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' && k < name_cap - 1)
    {
        name[k++] = *p++;
    }
    name[k] = '\0';
    /* 跳过 token 后面的非空白尾巴（如果 name 被截断，继续吃完这一段） */
    while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n')
        ++p;
    return p;
}

/* 可选：去重，避免同一任务被写入多次。若不需要，直接返回 0。*/
static int already_listed(const char *name, const char *out, unsigned used)
{
    return 0;
}

static int build_batch_script(char *line, char *out, unsigned out_cap)
{
    unsigned used = 0;
    char *p = line;
    char name[64];

    /* 如果需要：支持行首 # 注释
       p = skip_ws(p);
       if (*p == '#') return 0; */

    while (1)
    {
        p = skip_ws(p);
        if (!*p)
            break;

        p = next_token(p, name, (int)sizeof(name));
        if (name[0] == '\0')
            continue; /* 连续空白或超长被截断 */

        if (!task_exists(name))
        {
            bios_putstr("batch-write: no such task: ");
            bios_putstr(name);
            bios_putstr("\n\r");
            return -1;
        }

        unsigned n = (unsigned)strlen(name);
        if (used + n + 1 >= out_cap)
        {
            bios_putstr("batch-write: too long\n\r");
            return -1;
        }

        /* 复制 token 并添加空格分隔 */
        bytes_copy(out + used, name, (unsigned long)n);
        used += n;
        out[used++] = ' ';
    }

    /* 把结尾空格替换为换行 */
    if (used > 0)
    {
        if (out[used - 1] == ' ')
            out[used - 1] = '\n';
        else
        {
            if (used + 1 >= out_cap)
            {
                bios_putstr("batch-write: too long\n\r");
                return -1;
            }
            out[used++] = '\n';
        }
    }
    else
    {
        if (out_cap == 0)
            return -1;
        out[used++] = '\n';
    }

    return (int)used; /* 返回已写入字节数 */
}

static void batch_write(void)
{
    uint8_t bootsec[SECTOR_SIZE];
    if (sd_read((unsigned)(uintptr_t)bootsec, 1, 0) < 0)
    {
        bios_putstr("sd_read boot sector failed\n\r");
        return;
    }

    int batch_off = 0;
    bytes_copy(&batch_off, bootsec + BATCH_OFFSET_LOC, (unsigned long)sizeof(int));
    if (batch_off <= 0)
    {
        bios_putstr("no batch offset found\n\r");
        return;
    }

    char line[256];
    int len = 0;
    bios_putstr("Enter batch (task names, space separated): ");
    read_line_echo(line, (int)sizeof(line));

    bios_putstr("You entered: ");
    bios_putstr(line);
    bios_putstr("\n\r");

    static char out[BATCH_AREA_SIZE];
    bytes_set(out, 0, (unsigned long)sizeof(out));

    int used = build_batch_script(line, out, (unsigned)sizeof(out));
    if (used < 0)
        return;

    unsigned blk = (unsigned)(batch_off / SECTOR_SIZE);
    unsigned cnt = (unsigned)(BATCH_AREA_SIZE / SECTOR_SIZE);
    if (sd_write((unsigned)(uintptr_t)out, cnt, blk) < 0)
    {
        bios_putstr("batch-write: sd_write failed\n\r");
        return;
    }
    bios_putstr("batch written\n\r");
}

void print_int(int v)
{
    if (v == 0)
    {
        bios_putchar('0');
        return;
    }

    if (v < 0)
    {
        bios_putchar('-');
        v = -v;
    }

    char buf[12];
    int i = 0;
    while (v > 0)
    {
        buf[i++] = '0' + (v % 10);
        v /= 10;
    }

    while (i > 0)
        bios_putchar(buf[--i]); // 反向输出
}

static void batch_run(void)
{
    /* 1) 读取 boot 扇区并取出 batch 区偏移 */
    uint8_t bootsec[SECTOR_SIZE];
    if (sd_read((unsigned)(uintptr_t)bootsec, 1, 0) < 0)
    {
        bios_putstr("sd_read boot sector failed\n\r");
        return;
    }
    int batch_off = 0;
    bytes_copy(&batch_off, bootsec + BATCH_OFFSET_LOC, (unsigned long)sizeof(int));
    if (batch_off <= 0)
    {
        bios_putstr("no batch offset found\n\r");
        return;
    }
    *BATCH_MAILBOX_ADDR = 0;

    /* 2) 读取 batch 区脚本文本（固定 512B） */
    static char bbuf[BATCH_AREA_SIZE];
    bytes_set(bbuf, 0, (unsigned long)sizeof(bbuf));

    unsigned blk = (unsigned)(batch_off / SECTOR_SIZE);
    unsigned cnt = (unsigned)(BATCH_AREA_SIZE / SECTOR_SIZE);
    if (sd_read((unsigned)(uintptr_t)bbuf, cnt, blk) < 0)
    {
        bios_putstr("batch-run: sd_read failed\n\r");
        return;
    }

    /* 3) 逐 token 执行；支持 # 注释；遇错不断批，仅提示并继续 */
    char *p = bbuf;
    char name[64];

    while (1)
    {
        p = skip_ws(p);
        if (!*p)
            break;

        /* 行内注释：# 到行尾忽略 */
        if (*p == '#')
        {
            while (*p && *p != '\n' && *p != '\r')
                ++p;
            continue;
        }

        p = next_token(p, name, (int)sizeof(name));
        if (name[0] == '\0')
            continue; /* 空 token */

        if (!task_exists(name))
        {
            bios_putstr("batch-run: no such task: ");
            bios_putstr(name);
            bios_putstr("\n\r");
            continue; /* 不终止整批 */
        }

        bios_putstr("Run: ");
        bios_putstr(name);

        {
            uint64_t entry = load_task_img(name);
            if (!entry)
            {
                bios_putstr(" (load failed)\n\r");
                continue;
            }
            ((void (*)(void))entry)(); /* 任务返回后继续下一个 */
            bios_putstr(" [mailbox=");
            {
                int v = *BATCH_MAILBOX_ADDR;
                // 简易打印整数（用你已有的 int_to_str）
                print_int(v);
                bios_putstr("]");
            }
        }
        bios_putstr("\n\r");
    }

    bios_putstr("batch done\n\r");
}

static void print_task_names(void)
{
    bios_putstr("tasks-list:\n\r");
    for (int i = 0; i < 8; ++i)
    {
        if (tasks[i].name[0] != '\0')
        {
            bios_putstr(tasks[i].name);
            bios_putstr("\n\r");
        }
    }
}

static int read_token(char *buf, int cap)
{
    int n = 0;
    for (;;) {
        char ch = port_read_ch();

        if (ch == '\r' || ch == '\n') {        // 行结束
            buf[n] = '\0';
            return n;
        } else if ((ch == '\b' || (unsigned char)ch == 0x7F) && n > 0) {
            --n;                                // 退格
            port_write_ch('\b'); port_write_ch(' '); port_write_ch('\b');
        } else if (ch >= ' ' && ch <= '~' && n < cap - 1) {
            buf[n++] = ch;                      // 可打印字符
            port_write_ch(ch);
        }
        // 其他字符忽略
    }
}

void repl_loop(void)
{
    char task_name[32];

    for (;;) {
        bios_putstr("\n\rEnter task name: ");
        read_token(task_name, sizeof(task_name));
        bios_putstr("\n\r");

        if (!strcmp(task_name, "batch-write")) { batch_write(); continue; }
        if (!strcmp(task_name, "batch-run"))   { batch_run();   continue; }

        uint64_t entry = load_task_img(task_name);
        if (entry) ((void (*)(void))entry)();
        else bios_putstr("Failed to load task!\n\r");
    }
}

int main(void)
{
    // Check whether .bss section is set to zero
    int check = bss_check();
    // Init jump table provided by kernel and bios(ΦωΦ)
    init_jmptab();

    // Init task information (〃'▽'〃)
    init_task_info();

    // Init Process Control Blocks |•'-'•) ✧
    init_pcb();
    printk("> [INIT] PCB initialization succeeded.\n");

    // Read CPU frequency (｡•ᴗ-)_
    time_base = bios_read_fdt(TIMEBASE);

    // Init lock mechanism o(´^｀)o
    init_locks();
    printk("> [INIT] Lock mechanism initialization succeeded.\n");

    // Init interrupt (^_^)
    init_exception();
    printk("> [INIT] Interrupt processing initialization succeeded.\n");

    // Init system call table (0_0)
    init_syscall();
    printk("> [INIT] System call initialized successfully.\n");

    // Init screen (QAQ)
    init_screen();
    printk("> [INIT] SCREEN initialization succeeded.\n");

    // TODO: [p2-task4] Setup timer interrupt and enable all interrupt globally
    // NOTE: The function of sstatus.sie is different from sie's
    

    print_task_names();

    // Output 'Hello OS!', bss check result and OS version
    {
        char output_str[] = "bss check: _ version: _\n\r";
        char output_val[2] = {0};
        int i, pos = 0;

        output_val[0] = check ? 't' : 'f';
        output_val[1] = (char)(version + '0');

        for (i = 0; i < (int)sizeof(output_str); ++i)
        {
            buf[i] = output_str[i];
            if (buf[i] == '_')
                buf[i] = output_val[pos++];
        }

        bios_putstr("Hello OS!\n\r");
        bios_putstr(buf);
    }

    // TODO: Load tasks by either task id [p1-task3] or task name [p1-task4],
    //   and then execute them.

    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1)
    {
        // If you do non-preemptive scheduling, it's used to surrender control
        do_scheduler();

        // If you do preemptive scheduling, they're used to enable CSR_SIE and wfi
        // enable_preempt();
        // asm volatile("wfi");
    }
    repl_loop();

    /* 不会到达 */
    /* return 0; */
}
