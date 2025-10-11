#include <asm/biosdef.h>

// os size location (os_size could be sectors [p1-task3] or bytes [p1-task4])
.equ os_size_loc, 0x502001fc

// kernel address (move kernel to here, and jmp here to start kernel)
.equ kernel, 0x50201000

// BIOS function entry (jump here to use BIOS APIs)
.equ bios_func_entry, 0x50150000


.text
.global main

main:
	// fence on all memory and I/O
	fence

	// TODO: [p1-task1] call BIOS to print string "It's bootblock!"
	la 		a0,msg
	li  	a7,BIOS_PUTSTR
	jal		bios_func_entry


	// TODO: [p1-task2] call BIOS to read kernel in SD card
	la 		t0,os_size_loc
	lw		t0,0(t0)

	li		t1,511
	add		t0,t0,t1
	li		t1,512
	divu	t2,t0,t1
	
	div		t2,t0,t1
	rem		t3,t0,t1
	beqz	t3,rem_is_zero
	add 	t2,t2,1

rem_is_zero:
	la 		a0,kernel  //kernal address
	mv 		a1,t2
	li 		a2,1  //2nd bootblock
	li  	a7,BIOS_SDREAD
	jal 	bios_func_entry

	j 		kernal


	// TODO: [p1-task4] load task-related arguments and pass them to kernel


	// TODO: [p1-task2] jump to kernel to start UCAS-OS

	/************************************************************/
	/* Do not touch this comment. Reserved for future projects. */
	/************************************************************/
// while(1) --> stop here
stop:
	j stop
	nop

.data
msg: .string "It's SunJiahao's bootloader...\n\r"


