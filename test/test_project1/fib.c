#include <kernel.h>

static void bios_putint(int x){
    char buf[16]; int i=0;
    if (x==0){ bios_putchar('0'); return; }
    if (x<0){ bios_putchar('-'); x=-x; }
    while (x){ buf[i++] = '0' + (x%10); x/=10; }
    while (i--) bios_putchar(buf[i]);
}

int main(void){
    bios_putstr("[fib] Info: first 20 Fibonacci numbers\n\r");
    int a=0, b=1;
    for (int i=0; i<20; ++i){
        bios_putint(a); bios_putstr(i==19 ? "\n\r" : " ");
        int c = a + b;
        a = b; b = c;
    }

    /* 自检：F(19)=4181（以 F(0)=0,F(1)=1 计） */
    int x0=0,x1=1,x=0;
    for (int i=0;i<19;++i){ x=x0+x1; x0=x1; x1=x; }
    if (x1 != 4181){ bios_putstr("[fib] Error: self-check failed!\n\r"); while(1); }

    bios_putstr("[fib] OK!\n\r");
    return 0;
}
