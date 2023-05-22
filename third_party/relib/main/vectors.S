.section .UserEnter.text,"ax"
.org 0
.literal_position
	.literal .l_text_start, _text_start
	.literal .l_dispatch_table, 0x3fffc000

.global call_user_start
call_user_start:
	j call_user_start_local

/*
exceptions share the use of two exception vectors. These two vector addresses,
UserExceptionVector and KernelExceptionVector are both configuration options. The exception-handling process saves the address of the instruction causing the
exception into special register EPC[1] and the cause code for the exception into special
register EXCCAUSE. Interrupts and exceptions are precise, so that on returning from the
exception handler, program execution can continue exactly where it left off.
*/

.begin no-transform

.org 0x10
.global _DebugExceptionVector
_DebugExceptionVector:
	movi.n a2, 1
	j _panic

.org 0x20
/* A configured Xtensa LX processor can have as many as 6 level-sensitive or edge-triggered high-priority interrupts. One NMI or non-maskable interrupt can also be configured. The processor can have as many as six high-priority interrupt levels and each
high-priority interrupt level has its own interrupt vector. Each high-priority interrupt level
also has its own dedicated set of three special registers (EPC, EPS, and EXCSAVE) used
to save the processor state */
.global _NMIExceptionVector
_NMIExceptionVector:
	wsr	a3, EXCSAVE3
	j _NMILevelVector

.org 0x30
.global _KernelExceptionVector
_KernelExceptionVector:
	/* Exceptions with PS.UM == 0, PS.EXCM == 0 */
	/* The following should be compatible with the rom handlers */
	addmi	a1, a1, -0x100   /* reserve stack space, must match rom impl _xtos_return_from_exc  */
	s32i.n	a2, a1, 0x14
	s32i.n	a3, a1, 0x18
	s32i.n	a4, a1, 0x1c
	rsr	a2, EXCCAUSE     /* read into a2 since it is also the first handler argument */
	l32r	a3, .l_dispatch_table
	addx4	a3, a2, a3       /* calculate address of handler in dispatch table */
	l32i.n	a3, a3, 0        /* load handler address */
	jx	a3               /* jump to handler */

.org 0x50
.global _UserExceptionVector
_UserExceptionVector:
	/* Exceptions with PS.UM == 1, PS.EXCM == 0 */
	/* The following should be compatible with the rom handlers */
	addmi	a1, a1, -0x100   /* reserve stack space, must match rom impl _xtos_return_from_exc  */
	s32i.n	a2, a1, 0x14
	s32i.n	a3, a1, 0x18
	s32i.n	a4, a1, 0x1c
	rsr	a2, EXCCAUSE     /* read into a2 since it is also the first handler argument */
	l32r	a3, .l_dispatch_table
	addx4	a3, a2, a3       /* calculate address of handler in dispatch table */
	l32i.n	a3, a3, 0        /* load handler address */
	jx	a3               /* jump to handler */

.org 0x70
.global _DoubleExceptionVector
_DoubleExceptionVector:
	movi.n a2, 7
	j _panic

.org 0x80
_ResetVector: /* really unused here, since reset should jump to ROM directly */
	movi.n a2, 8
	j _panic

.end no-transform

.section .text,"ax"
.literal_position
	.literal .l_user_start, user_start
	.literal .l_ets_run, ets_run
	.literal .l_panic_stack, 0x40000000
	.literal .l_panic_handler, panic_handler

_panic:
	movi.n a1, 0
	wsr a1, LITBASE
	esync
	l32r a1, .l_panic_stack
	rsr  a3, EPC1
	rsr  a4, DEPC
	l32r a0, .l_panic_handler
	jx a0

call_user_start_local:
	l32r	a0, .l_text_start
	wsr	a0, VECBASE
	l32r	a0, .l_user_start
	callx0	a0
	l32r	a0, .l_ets_run
	jx	a0
