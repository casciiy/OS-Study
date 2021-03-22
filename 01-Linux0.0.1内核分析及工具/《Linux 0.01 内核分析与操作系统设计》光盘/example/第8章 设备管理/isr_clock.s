/* isr_clock.s */
	.globl   _clock_isr
	.align   4
	
_clock_isr:

    /* save some registers */
	pushl	%eax
	pushl	%ds
	pushl	%es
	pushl	%fs
	pushl	%gs

    /* set our descriptors up to a DATA selector */
	movl    $0x10, %eax
	movw    %ax, %ds
	movw    %ax, %es
	movw    %ax, %fs
	movw    %ax, %gs

    /* call our clock routine */
	call	_ISR_clock

    /* clear the PIC (clock is a PIC interrupt) */
	movl    $0x20, %eax
	outb    %al, $0x20

    /* restor our regs */
	popl	%gs
	popl	%fs
	popl	%es
	popl	%ds
	popl	%eax
	iret
