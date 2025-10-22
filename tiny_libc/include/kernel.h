#ifndef __INCLUDE_KERNEL_H__
#define __INCLUDE_KERNEL_H__

#include <stdint.h>

#define KERNEL_JMPTAB_BASE 0x51ffff00
typedef enum {
    CONSOLE_PUTSTR,
    CONSOLE_PUTCHAR,
    CONSOLE_GETCHAR,
    SD_READ,
    SD_WRITE,
    QEMU_LOGGING,
    SET_TIMER,
    READ_FDT,
    MOVE_CURSOR,
    PRINT,
    YIELD,
    MUTEX_INIT,
    MUTEX_ACQ,
    MUTEX_RELEASE,
    NUM_ENTRIES
} jmptab_idx_t;


static inline long call_jmptab(long which, long arg0, long arg1, long arg2, long arg3, long arg4)
{
    unsigned long val = \
        *(unsigned long *)(KERNEL_JMPTAB_BASE + sizeof(unsigned long) * which);
    long (*func)(long, long, long, long, long) = (long (*)(long, long, long, long, long))val;

    return func(arg0, arg1, arg2, arg3, arg4);
}

static inline void bios_putstr(char *str)
{
    call_jmptab(CONSOLE_PUTSTR, (long)str, 0, 0, 0, 0);
}

static inline void bios_putchar(int ch)
{
    call_jmptab(CONSOLE_PUTCHAR, (long)ch, 0, 0, 0, 0);
}

static inline int bios_getchar(void)
{
    return call_jmptab(CONSOLE_GETCHAR, 0, 0, 0, 0, 0);
}

// --- add: tiny print helpers (only use bios_putchar / bios_putstr) ---
static void putch(char c){ bios_putchar(c); }

static void put_dec(int x){
    char buf[16]; int i=0;
    if(x==0){ putch('0'); return; }
    if(x<0){ putch('-'); x=-x; }
    while(x){ buf[i++] = '0'+(x%10); x/=10; }
    while(i--) putch(buf[i]);
}

static void put_hex8(unsigned int v){
    const char* H="0123456789ABCDEF";
    putch(H[(v>>4)&0xF]); putch(H[v&0xF]);
}

static void put_hex32(unsigned int v){
    for(int i=7;i>=0;--i) put_hex8((v>>(i*4))&0xF);
}

static void dump_bytes(const void* p, int n){
    const unsigned char* b=(const unsigned char*)p;
    for(int i=0;i<n;i++){
        put_hex8(b[i]); putch(' ');
        if((i&0x0F)==0x0F){ bios_putstr("\n"); }
    }
    if((n&0x0F)!=0) bios_putstr("\n");
}


#endif