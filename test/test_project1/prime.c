#include <kernel.h>

static void bios_putint(int x){
    char buf[16]; int i=0;
    if (x==0){ bios_putchar('0'); return; }
    if (x<0){ bios_putchar('-'); x=-x; }
    while (x){ buf[i++] = '0' + (x%10); x/=10; }
    while (i--) bios_putchar(buf[i]);
}

static int is_prime(int n){
    if (n<2) return 0;
    if (n==2 || n==3) return 1;
    if (n%2==0) return 0;
    for (int d=3; d*d<=n; d+=2) if (n%d==0) return 0;
    return 1;
}

int main(void){
    bios_putstr("[prime] Info: listing primes <= 100\n\r");
    int count=0;
    for (int n=2; n<=100; ++n){
        if (is_prime(n)){
            bios_putint(n); bios_putstr(" ");
            ++count;
        }
    }
    bios_putstr("\n\r[prime] Count = "); bios_putint(count); bios_putstr("\n\r");

    /* 简单自检：前25个质数之和应为 1060（2..97） */
    int sum=0, c=0;
    for (int n=2; c<25; ++n){ if (is_prime(n)){ sum+=n; ++c; } }
    if (sum!=1060){ bios_putstr("[prime] Error: self-check failed!\n\r"); while(1); }
    bios_putstr("[prime] OK!\n\r");
    return 0;
}
