#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <type.h>
#include <common.h>

extern task_info_t tasks[TASK_MAXNUM];

static void putstr(const char *s)
{
    while (*s)
        port_write_ch(*s++);
}
static void put_dec_u(unsigned int v)
{
    char b[11];
    int n = 0;
    if (!v)
    {
        port_write_ch('0');
        return;
    }
    while (v)
    {
        b[n++] = '0' + (v % 10);
        v /= 10;
    }
    while (n--)
        port_write_ch(b[n]);
}
static void put_hex8(unsigned char x)
{
    static const char H[] = "0123456789ABCDEF";
    port_write_ch(H[(x >> 4) & 0xF]);
    port_write_ch(H[x & 0xF]);
}
static void put_hex32(unsigned int x)
{
    for (int i = 7; i >= 0; --i)
    {
        unsigned int nyb = (x >> (i * 4)) & 0xF;
        static const char H[] = "0123456789ABCDEF";
        port_write_ch(H[nyb]);
    }
}

static inline unsigned ceil_div_u(unsigned x, unsigned y)
{
    return (x + y - 1) / y;
}

uint64_t load_task_img(const char *taskname)
{
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */

    for (int i = 0; i < TASK_MAXNUM; ++i)
    {
        if (strcmp(tasks[i].name, taskname) != 0)
            continue;

        bios_putstr("\n\rLoading task: ");
        bios_putstr(tasks[i].name);
        bios_putstr("\n\r");

        unsigned task_entry = (unsigned)(TASK_MEM_BASE + TASK_SIZE * i);
        unsigned block_id = tasks[i].offset / SECTOR_SIZE;
        unsigned block_i_off = tasks[i].offset % SECTOR_SIZE;

        unsigned total_bytes = block_i_off + tasks[i].size;
        unsigned num_blks = ceil_div_u(total_bytes, SECTOR_SIZE);

        if (sd_read(task_entry, num_blks, block_id) < 0)
        {
            bios_putstr("\n\rFailed to load task!");
            return 0;
        }
        return (uint64_t)task_entry + block_i_off;
    }

    bios_putstr("\n\rTask not found!");
    return 0;
}
