#include <assert.h>
#include "c_types.h"
#include "spi_flash.h"
#include "osapi.h"
#include "mem.h"

#include "relib/esp8266.h"
#include "relib/xtensa.h"
#include "relib/ets_rom.h"
#include "relib/relib.h"

/* modified version of ets/xtos exception frame */
typedef struct {
	uint32_t epc;      // 0x00
	uint32_t ps;       // 0x04
	uint32_t sar;      // 0x08
	union {
		uint32_t reg[16];  // 0x0c
		struct {
			uint32_t a0;
			uint32_t a1;
			uint32_t a2;
			uint32_t a3;
			uint32_t a4;
			uint32_t a5;
			uint32_t a6;
			uint32_t a7;
			uint32_t a8;
			uint32_t a9;
			uint32_t a10;
			uint32_t a11;
			uint32_t a12;
			uint32_t a13;
			uint32_t a14;
			uint32_t a15;
		};
	};
	uint32_t cause;    // 0x4c
	uint32_t vpri;     // 0x50
} modified_exception_frame_t;

enum trace_state {
	TRACE_OFF = 0,
	TRACE_START = 1,
	TRACE_ON = 2,
	TRACE_STOP = 3,
};

enum debug_cause {
	DBG_ICOUNT  = 1,  /* ICOUNT reached -1 */
	DBG_IBREAK  = 2,  /* hw instr breakpoint */
	DBG_DBREAK  = 4,  /* hw data breakpoint */
	DBG_BREAK   = 8,  /* break instruction (3 byte, 2 arg) */
	DBG_BREAKN  = 16, /* break.n instruction (2 byte, 1 arg) */
	DBG_OCD_INT = 32, /* OCD debugger interrupt */
};

static enum trace_state trace_state = TRACE_OFF;

static uint32_t
read_insn(uint32_t addr)
{
	uint32_t val = UNALIGNED_L32(addr);
	uint32_t mask = CONST_ONES_SHIFTR(8); /* 0x00ffffff */
	if (val & 8) { /* 2-byte insn */
		mask >>= 8;
	}
	return val & mask;
}

bool ICACHE_FLASH_ATTR
decode_loadstore_wide(int imm8, int r, int s, int t, uint32_t *regs)
{
	/* r == 3 is reserved, not checking that case */
	if (r > 6 && r != 9) {
		return false;
	}
	int dis_shift = r & 3;
	int dis = imm8 << dis_shift;
	int bits = 8 << dis_shift;
	uint32_t mask = dis_shift == 2 ? -1 : ~(0xffffffffU << bits);
	int signext = r == 9;
	char *suffix = signext ? "S" : "U";
	uint32_t addr = regs[s] + dis;
	if (r & 4) { /* Store */
		uint32_t v = regs[t] & mask;
		ets_printf("S%dI a%d, a%d, %d ; [@%08x]=%08x",
			bits, t, s, dis, addr, v);
	} else { /* Load */
		ets_printf("L%d%sI a%d, a%d, %d ; [@%08x]",
			bits, suffix, t, s, dis, addr);
		/* TODO: At this point the insn isn't executed yet, so we emulate it
		 * However this means we'll do the load twice, which for hw regs
		 * can mess up hw state for "reset on read" registers.
		 * This could be fixed by fully emulating the instruction
		 * (updating saved registers, pc and icount) */
		if (addr >= 0x20000000 && addr < 0x80000000) {
			uint32_t v = UNALIGNED_L32(addr) & mask;
			ets_printf("=%08x", v);
		}
	}
	return true;
}

static bool ICACHE_FLASH_ATTR
decode_loadstore(uint32_t insn, uint32_t *regs)
{
	/* RRI8: imm8 | r | s | t | op0
	 * RRRN:        r | s | t | op0
	 * L8UI:   at, as, 0..255   ; imm8 | 0 | s | t | 2
	 * L16UI:  at, as, 0..510   ; imm8 | 1 | s | t | 2
	 * L32I:   at, as, 0..1020  ; imm8 | 2 | s | t | 2
	 * S8I:    at, as, 0..255   ; imm8 | 4 | s | t | 2
	 * S16I:   at, as, 0..510   ; imm8 | 5 | s | t | 2
	 * S32I:   at, as, 0..1020  ; imm8 | 6 | s | t | 2
	 * L16SI:  at, as, 0..510   ; imm8 | 9 | s | t | 2
	 * L32I.N: at, as, 0..60    ;     imm4 | s | t | 8
	 * S32I.N  at, as, 0..60    ;     imm4 | s | t | 9
	 */
	int op0 = insn & 0xf;
	int t = (insn >> 4) & 0xf;
	int s = (insn >> 8) & 0xf;
	int r = (insn >> 12) & 0xf;
	int imm8 = insn >> 16;

	switch (op0) {
	default: return false;
	case 2:  return decode_loadstore_wide(imm8, r, s, t, regs);
	case 8:  break;
	case 9:  break;
	};

	int dis = 4 * r;
	uint32_t addr = regs[s] + dis;
	if (op0 & 1) {
		ets_printf("S32I.N a%d, a%d, %d ; [@%08x]=%08x",
			t, s, dis, addr, regs[t]);
	} else {
		ets_printf("L32I.N a%d, a%d, %d ; [@%08x]",
			t, s, dis, addr);
		/* TODO: At this point the insn isn't executed yet, so we emulate it
		 * However this means we'll do the load twice, which for hw regs
		 * can mess up hw state for "reset on read" registers.
		 * This could be fixed by fully emulating the instruction
		 * (updating saved registers, pc and icount)  */
		if (addr >= 0x20000000 && addr < 0x80000000) {
			ets_printf("=%08x", UNALIGNED_L32(addr));
		}
	}
	return true;
}

void handle_debug_exception(modified_exception_frame_t *ef, int cause)
{
	if (cause & DBG_IBREAK) {
		cause &= ~DBG_IBREAK;
		if (trace_state == TRACE_START) {
			/* Entry to traced function */
			ets_printf("Trace start @%08x , until @%08x\n", ef->epc, ef->a0);
			trace_state = TRACE_ON;
			/* Next IBREAK on return from traced function */
			WSR(IBREAKA0, ef->a0);
			/* Enable single-stepping */
			WSR(ICOUNTLEVEL, 1);
			WSR(ICOUNT, -2);
			/* Fake ICOUNT to trace first instruction */
			cause |= DBG_ICOUNT;
		} else {
			/* Returned from traced function */
			ets_printf("Trace end @%08x\n", ef->epc);
			trace_state = TRACE_OFF;
			/* Remove hw breakpoint */
			WSR(IBREAKENABLE, 0);
			/* Disable single-stepping */
			WSR(ICOUNTLEVEL, 0);
			return;
		}
	}

	if ((cause & DBG_ICOUNT) && trace_state == TRACE_ON) {
		uint32_t addr = ef->epc;

		/* Reset ICOUNT for singlestepping into next instruction */
		WSR(ICOUNT, -2);

		if (addr < 0x40100000) {
			/* Skip ROM calls when tracing. */
			return;
		}

		uint32_t insn = read_insn(ef->epc);
		if (decode_loadstore(insn, ef->reg)) {
			ets_printf("  @pc=%08x\n", ef->epc);
		}
		return;
	}

	/* Default handler */
	ets_printf("Debug exception %d\n", cause);
	for (int i = 0; i < 16; i++) {
		if (i > 1 && i <8) {
			/* Align regs */
			ets_printf(" ");
		}
		ets_printf(" a%d=%08x", i, ef->reg[i]);
		if ((i & 7) == 7) {
			ets_printf("\n");
		}
	};
	ets_printf(" epc=%08x ps=%08x sar=%d\n", ef->epc, ef->ps, ef->sar);
	while (1);
}

void
debug_trace(uint32_t fnaddr)
{
	uint32_t saved = LOCK_IRQ_SAVE();

	trace_state = TRACE_START;
	WSR(IBREAKA0, fnaddr);
	WSR(IBREAKENABLE, 1);

	LOCK_IRQ_RESTORE(saved);
}
