// 4.c
#include <kernel.h>
#define BATCH_MAILBOX_ADDR ((volatile int *)0x50510000)

static void print_int(int x)
{
    char buf[16];
    int i = 0;
    if (x == 0)
    {
        bios_putchar('0');
        return;
    }
    if (x < 0)
    {
        bios_putchar('-');
        x = -x;
    }
    while (x)
    {
        buf[i++] = (char)('0' + (x % 10));
        x /= 10;
    }
    while (i--)
        bios_putchar(buf[i]);
}

int main(void)
{
    int in = *BATCH_MAILBOX_ADDR;
    int out = in * in;
    *BATCH_MAILBOX_ADDR = out;

    bios_putstr("[4] input=");
    print_int(in);
    bios_putstr(", square => ");
    print_int(out);
    bios_putstr("\n\r");
    return 0;
}
