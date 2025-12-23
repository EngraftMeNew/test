#include <kernel.h>

int main(void)
{
    bios_putstr("[loop] Info: infinite loop demo\n\r");
    for (int i = 0;i<=5; ++i) {
        bios_putstr("[loop] Tick ");
        bios_putchar('0' + (i % 10));
        bios_putstr("\n\r");
        for (volatile int d = 0; d < 500000; ++d);  // 简单延时
    }
    return 0;
}
