#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <type.h>
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

uint64_t load_task_img(int taskid)
{
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */

    /*task3*/
    uint64_t EntryAddr;
    char output[] = "loading task X now \n";
    for (int i = 0; i < strlen(output); i++)
    {
        if (output[i] == 'X')
            bios_putchar((char)(taskid + '0'));
        else
            bios_putchar(output[i]);
    }
    EntryAddr = TASK_MEM_BASE + TASK_SIZE * (taskid - 1);
    putstr("[loader] Entry=");
    put_hex32((unsigned)EntryAddr);
    putstr(" LBA=");
    put_dec_u(1 + taskid * 32); // 若你已把“15”都改成“20”，这里要一致！


    bios_sd_read(EntryAddr, 32, 1 + taskid * 32);

    putstr("[loader] Peek16 ");
    unsigned char *p = (unsigned char *)EntryAddr;
    for (int i = 0; i < 16; i++)
    {
        put_hex8(p[i]);
        port_write_ch(' ');
    }
    port_write_ch('\n');

    bios_sd_read(EntryAddr, 32 , 1 + taskid * 32);
    return EntryAddr;
}