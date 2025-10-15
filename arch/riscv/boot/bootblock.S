#include <asm/biosdef.h>

// os size location (os_size could be sectors [p1-task3] or bytes [p1-task4])
.equ OS_SIZE_LOC,       0x502001fc 

.equ TASKNUM_LOC,       0x502001f6     // uint16_t: task count 

.equ APPINFO_OFFSET_LOC,0x502001f8     // uint32_t: appinfo offset 

// kernel address (move kernel to here, and jmp here to start kernel)
.equ KERNEL_ADDR,       0x50201000

.equ BIOS_ENTRY,        0x50150000

.equ SECTOR_SIZE,       512

.data
msg: .string "It's SunJiahao's bootblock...\n\r"

.text
.global main

main:
    // fence on all memory and I/O
    fence

    // TODO: [p1-task1] call BIOS to print string "It's bootblock!"
    la      a0, msg
    li      a7, BIOS_PUTSTR
    la      t0, BIOS_ENTRY
    jalr    t0

    // TODO: [p1-task2] call BIOS to read kernel in SD card

    # a0 = 目标内存地址
    la      a0, KERNEL_ADDR

    # 读取 OS 字节数
    la      t1, OS_SIZE_LOC
    lh      a1, 0(t1)                 # a1 = os_size(bytes), 16-bit

    # 字节 -> 扇区（向上取整）
    li      t2, SECTOR_SIZE           # t2 = 512
    addi    a1, a1, SECTOR_SIZE-1     # a1 += 511
    div     a1, a1, t2                # a1 = ceil(bytes/512)

    # a2 = 起始 LBA 扇区号（第二个扇区）
    li      a2, 1

    li      a7, BIOS_SDREAD
    la      t0, BIOS_ENTRY
    jalr    t0

    # 把 appinfo 参数传给内核
    # 从 0x502001F8 读取 4 字节的 appinfo_off（offset）
    # 从 0x502001F6 读取 2 字节的 tasknum
    la      t1, APPINFO_OFFSET_LOC
    lw      a0, 0(t1)                 # a0 = appinfo_off
    la      t1, TASKNUM_LOC
    lh      a1, 0(t1)                 # a1 = tasknum（如果内核不需要，可不传）

    # 跳到 kernel
    la      t0, KERNEL_ADDR
    jalr    t0

stop:
    j       stop
    nop
