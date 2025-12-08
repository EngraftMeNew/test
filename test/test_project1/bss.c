#include <kernel.h>

#define BUF_LEN 50

char buf[BUF_LEN];

int main(void)
{
    bios_putstr("[bss] Debug: buf base = 0x");
    put_hex32((unsigned int)(unsigned long)(&buf[0]));
    bios_putstr(", end = 0x");
    put_hex32((unsigned int)(unsigned long)(&buf[BUF_LEN - 1]));
    bios_putstr(", len = ");
    put_dec(BUF_LEN);
    bios_putstr("\n");

    // do bss_check
    int check_ok = 1;
    for (int i = 0; i < BUF_LEN; ++i)
    {
        if (buf[i] != 0)
        {
            bios_putstr("[bss] Debug: first nonzero @ i = ");
            put_dec(i);
            bios_putstr(", value = 0x");
            put_hex8((unsigned char)buf[i]);
            bios_putstr("\n");

            int start = (i - 16 < 0) ? 0 : (i - 16);
            int count = ((i + 16) >= BUF_LEN) ? (BUF_LEN - start) : (i + 16 - start + 1);

            bios_putstr("[bss] Debug: dump buf[start..] (start=");
            put_dec(start);
            bios_putstr(", count=");
            put_dec(count);
            bios_putstr(")\n");
            dump_bytes(&buf[start], count);

            check_ok = 0;
            break;
        }
    }

    if (check_ok)
    {
        bios_putstr("[bss] Info: passed bss check!\n");
    }
    else
    {
        bios_putstr("[bss] Error: bss check failed!\n");
        bios_putstr("[bss] Hint: see above for first-nonzero index and local dump.\n");
    }

    return 0;
}