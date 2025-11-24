// 2.c
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
    int in = *BATCH_MAILBOX_ADDR; // 读上一个的输出
    int out = in + 10;            // 处理
    *BATCH_MAILBOX_ADDR = out;    // 回写，供下一个使用

    bios_putstr("[2] input=");
    print_int(in);
    bios_putstr(", +10 => ");
    print_int(out);
    bios_putstr("\n\r");
    return 0;
}
