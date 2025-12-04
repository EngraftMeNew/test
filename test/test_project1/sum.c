#include <kernel.h>

void bios_putint(int x)
{
    char buf[16];
    int i = 0;
    if (x == 0) { bios_putchar('0'); return; }
    if (x < 0) { bios_putchar('-'); x = -x; }
    while (x) { buf[i++] = '0' + (x % 10); x /= 10; }
    while (i--) bios_putchar(buf[i]);
}

int main(void)
{
    bios_putstr("[sum] Info: computing sum 1..10\n\r");
    int sum = 0;
    for (int i = 1; i <= 10; ++i) sum += i;
    bios_putstr("[sum] Result = "); bios_putint(sum); bios_putstr("\n\r");
    bios_putstr("[sum] Done.\n\r");
    return 0;
}
