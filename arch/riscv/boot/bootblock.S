#include <asm/biosdef.h>
#include <csr.h>

// os size location (os_size could be sectors [p1-task3] or bytes [p1-task4])
.equ os_size_loc, 0x502001fc

// kernel address (move kernel to here, and jmp here to start kernel)
//.equ kernel, 0x50202000

// BIOS function entry (jump here to use BIOS APIs)
.equ bios_func_entry, 0x50150000
.equ SECTOR_SIZE,       512
// start address for task info is stored in this loacation, size of array is in following 4 bytes
.equ app_info_addr_loc, 0x502001f4

.equ KERNEL_VA, 0xffffffc050202000
.equ KERNEL_PA, 0x50202000

.text
.global main

main:
	// fence on all memory and I/O
	fence
    // a0 is mhartid
	bnez a0, secondary

	// call BIOS to print string "It's bootblock!"
	la a0, msg
	li a7, BIOS_PUTSTR
	jal bios_func_entry

	// call BIOS to read kernel in SD card
	li a7, BIOS_SDREAD
	//la a0, kernel
	li a0, KERNEL_PA
	la t0, os_size_loc
	lh a1, 0(t0)
	li a2, 1
	jal bios_func_entry



	// load task-related arguments and pass them to kernel
	la t1, app_info_addr_loc	
	lw a0, (t1)		// pass the location for task info as parameter 1
	lw a1, 4(t1)	// pass the size as parameter 2

	//  jump to kernel to start UCAS-OS
	//jal kernel
	li t0, KERNEL_PA
	jalr zero, t0, 0


	/************************************************************/
secondary:
	/* TODO [P3-task3]: 
	 * 1. Mask all interrupts
	 * 2. let stvec pointer to kernel_main
	 * 3. enable software interrupt for ipi
	 */
	li t0, SR_SIE
  	csrc CSR_SSTATUS, t0


	//la t0, kernel
	la t0, KERNEL_PA
	csrw stvec, t0
	li t0, SIE_SSIE
	csrs sie, t0		
	li t0, SR_SIE
	csrs sstatus, t0	
wait_for_wakeup:
	wfi
	j wait_for_wakeup

	/************************************************************/
// while(1) --> stop here
stop:
	j stop
	nop

.data

msg: .string "It's SunJiahao's bootloader...\n\r"

