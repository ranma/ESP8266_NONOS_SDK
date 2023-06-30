#include <stdint.h>

#include "c_types.h"
#include "osapi.h"

#include "relib/esp8266.h"
#include "relib/xtensa.h"
#include "relib/relib.h"
#include "relib/ets_rom.h"
#include "relib/ets_timer.h"

static ETSEvent rtcTimerEvtQ[4];

ETSTimer* timer_list;  /* fpm_onEtsIdle and pm_onEtsIdle reference this directly! */
extern uint8_t timer2_ms_flag;  /* from user_interface.o */

#if 0  /* same as rom impl, no need to override */
void
ets_timer_setfn(ETSTimer *t, ETSTimerFunc *fn, void *parg)
{
	t->timer_func = fn;
	t->timer_arg = parg;
	t->timer_expire = 0;
	t->timer_period = 0;
	t->timer_next = (ETSTimer*)-1;
}
#endif

void
ets_rtc_timer_arm(uint32_t timer_expire)
{
#if 0
	uint32_t cc1, cc2, base, ps, future, diff;
	/* Critical section is measured to take 19 cycles (including CCOUNT reads)
	 * in the fast case (jump not taken).
	 * Out of these, 5 cycles are for instructions that should take
	 * 1 cycle each, leaving 14 cycles for the peripheral
	 * load+store (presumably 7 cycles each).
	 */
	__asm__ __volatile__(
		/* Load FRC2 base without using L32R */
		"movi.n  %[base], 6\n"
		"slli    %[base], %[base], 28\n"
		"addmi   %[base], %[base], 0x620\n"
		/* Start of critical section */
		"rsil    %[ps], 3\n"
		"rsr     %[cc1], CCOUNT\n"
		/* Load FRC2->COUNT */
		"l32i.n  %[tf], %[base], 0x04\n"
		/* Add safety margin */
		"addi    %[tf], %[tf], 20\n"
		/* Check if expiry is soon or already past */
		"sub     %[d], %[te], %[tf]\n"
		/* Jump if expiry is too close or already past */
		"bgei    %[d], 1, .erta_skip\n"
		/* Expiry far enough into the future, use original value */
		"mov.n   %[tf], %[te]\n"
		".erta_skip:\n"
		/* Write new target time to FRC2->ALARM */
		"s32i.n  %[tf], %[base], 0x10\n"
		/* Leave critical section */
		"rsr     %[cc2], CCOUNT\n"
		"wsr     %[ps], PS\n"
		: [cc1]"=a"(cc1),
		  [cc2]"=a"(cc2),
		  [ps]"=a"(ps),
		  [tf]"=a"(future),
		  [d]"=a"(diff),
		  [base]"=&a"(base)
		: [te]"a"(timer_expire)
		:
	);

/*
	static int arm_cycles = 0;
	int cc_diff = cc2 - cc1;
	if (cc_diff != arm_cycles) {
		ets_printf("ets_rtc_timer_arm: %d critical cycles\n", cc_diff);
		ets_printf("base=%08x ps=%08x tf=%08x te=%08x d=%d\n",
			base, ps, future, timer_expire, diff);
		arm_cycles = cc_diff;
	}
*/

#else
	/* Margin for reg read, TIME_AFTER, adjustment and reg write
	 * 5 * (80Mhz / 5MHz) => 80 cycles
	 */
	const int safety_margin = 5;
	static int total_calls = 0;
	static int total_adjusts = 0;
	int adjusted = 0;

	total_calls++;

	/* Critical section, so disable irqs */
	uint32_t saved = LOCK_IRQ_SAVE();
	/* Write target value _first_ */
	FRC2->ALARM = timer_expire;
	/* Then get current count to check if it already expired */
	int t_now = FRC2->COUNT + safety_margin;
	if (TIME_AFTER(t_now, timer_expire)) {
		/* timer close to expiry, reschedule to near future to
		 * make sure the interrupt is fired */
		FRC2->ALARM = t_now;
		/* TODO: If there is a way to manually set the interrupt flag,
		 * no safety_margin would be needed here */
		adjusted = 1;
		total_adjusts++;
	}
	/* Restore irqs */
	LOCK_IRQ_RESTORE(saved);

	if (adjusted) {
		ets_printf("ets_rtc_timer_arm: used %08x instead of %08x (%d/%d)\n",
			t_now, timer_expire,
			total_adjusts, total_calls);
	}
#endif
}

void
ets_timer_intr_set(uint32_t timer_expire)
{
	/* ets_rtc_timer_arm already handles expiry time close to
	 * current time, so special handling removed here */
	ets_rtc_timer_arm(timer_expire);
}

/* Overrides bootrom function of the same name */
void
timer_insert(uint32_t timer_expire, ETSTimer *ptimer)
{
	ETSTimer *t_before = NULL;
	ETSTimer *t_after;

	t_after = timer_list;
	if (timer_list != NULL) {
		ETSTimer *t_tmp;
		do {
			t_tmp = t_after;
			t_after = t_tmp;
		} while ((0 < (int)(timer_expire - t_tmp->timer_expire)) &&
						(t_after = t_tmp->timer_next, t_before = t_tmp, t_after != NULL));
	}
	ptimer->timer_next = t_after;
	ptimer->timer_expire = timer_expire;
	if (t_before == (ETSTimer *)0x0) {
		timer_list = ptimer;
		ets_rtc_timer_arm(timer_expire);
	}
	else {
		t_before->timer_next = ptimer;
	}
	if (ptimer->timer_next != ptimer) {
		return;
	}
	ets_printf("%s %u\n","tim",222);
	while(1);
}

void ICACHE_FLASH_ATTR
ets_timer_handler_isr(void)
{
	uint32_t saved;
	uint32_t now_us;
	uint32_t frc2_now;
	ETSTimer *t;

	saved = LOCK_IRQ_SAVE();
	/* Read the seperate WDEV microsecond timer (counts up) */
	now_us = WDEV->MICROS;
	do {
		/* FRC2 is a 32-bit counter that counts up, with higher resolution than the WDEV timer */
		frc2_now = FRC2->COUNT;
		t = timer_list;
		if (t == NULL) {
LAB_40230a24:
			LOCK_IRQ_RESTORE(saved);
			return;
		}
		/* If the timer at the head of the timer list is not expired yet, re-arm with the expiry time */
		if (0 < (int)(timer_list->timer_expire - frc2_now)) {
			ets_rtc_timer_arm(timer_list->timer_expire);
			goto LAB_40230a24;
		}
		/* Timer is expired */

		/* Remove from timer_list */
		timer_list = t->timer_next;
		t->timer_next = (ETSTimer *)-1;

		/* If the timer is periodic, update the expiry time and re-insert it into the timer list */
		if (t->timer_period != 0) {
			/* It's odd that this is checking against the wdev timer */
			/* If we have spent more than 15ms in the handler loop, override the frc2 count reading? */
			if ((uint32_t)(WDEV->MICROS - now_us) < 15001) {
				frc2_now = t->timer_expire;
			}
			/* Update the timer expiry time by adding the period to the FRC2 count we read above */
			uint32_t new_expire = frc2_now + t->timer_period;
			t->timer_expire = new_expire;

			/* And re-insert the timer */
			timer_insert(new_expire, t);
		}
		/* Call timer func with IRQs unlocked */
		LOCK_IRQ_RESTORE(saved);
		t->timer_func(t->timer_arg);
		saved = LOCK_IRQ_SAVE();
	} while (1);
}

void ICACHE_FLASH_ATTR
ets_rtc_timer_task(ETSEvent *e)
{
	if (e->sig != 0) {
		return;
	}
	ets_timer_handler_isr();
}

void ets_timer_handler_dsr(void *unused_arg)
{
	ets_post(PRIO_RTC_TIMER, 0, 0);
}

void ICACHE_FLASH_ATTR
ets_timer_init(void)
{
	timer_list = NULL;
	/* ets_isr_attach is just a no-op wrapper for * _xtos_set_interrupt_handler_arg */
	_xtos_set_interrupt_handler_arg(10, ets_timer_handler_dsr, NULL);

	DPORT->EDGE_INT_ENABLE |= 4;
	_xtos_ints_on(0x400);

	ets_task(ets_rtc_timer_task, PRIO_RTC_TIMER, rtcTimerEvtQ, ARRAY_SIZE(rtcTimerEvtQ));
	FRC2->ALARM = 0;
	FRC2->CTRL = 0x88;  /* FRC2 @312.5kHz (80MHz / 256) */
	FRC2->LOAD = 0;
}

void
ets_timer_disarm(ETSTimer *ptimer)
{
	uint32_t saved = LOCK_IRQ_SAVE();
	if (timer_list == ptimer) {
		/* ptimer is the pending timer, update FRC2 alarm */
		timer_list = ptimer->timer_next;
		ets_rtc_timer_arm(timer_list->timer_expire);
	} else {
		/* an earlier timer is still pending, just remove ptimer from the list */
		ETSTimer **p = &timer_list;
		while (*p) {
			if (*p == ptimer) {
				/* update entry pointer */
				*p = ptimer->timer_next;
				break;
			}
			/* inspect next list entry */
			p = &((*p)->timer_next);
		}
	}
	ptimer->timer_next = (ETSTimer*)-1;
	ptimer->timer_period = 0;
	LOCK_IRQ_RESTORE(saved);
}

void
ets_timer_arm_new(ETSTimer *ptimer, uint32_t time, bool repeat_flag, bool ms_flag)
{
	uint32_t period;
	int t_now;
	
	if (ptimer->timer_next != (ETSTimer *)-1) {
		ets_timer_disarm(ptimer);
	}
	if (ptimer->timer_func == NULL) {
		os_printf_plus("timer:%p cb is null\n",ptimer);
		return;
	}
	if (timer2_ms_flag == '\0') {
		if (ms_flag) {
			if (428496 /* 2^32 / 10000 */ < time) {
				os_printf_plus("err1,exceed max time value\n");
				return;
			}
			time = time * 1000;
		}
		else if (0x198a5759 /* 2^32 / 10 */ < time) {
			os_printf_plus("err2,exceed max time value\n");
			return;
		}
		if (time == 0) {
			period = 0;
		}
		else if (time < 859) {
			period = time * 5;
		}
		else {
			period = ((time & 0xfffffffc) + (time >> 2)) * 4 + (time & 3) * 5;
		}
	}
	else {
		if (0x68d7a3 < time) {
			os_printf_plus("err3,exceed max time value\n");
			return;
		}
		if (time == 0) {
			period = 0;
		}
		else if (time < 13744) {
			period = (time * 625) / 2;
		}
		else {
			period = (time & 3) * 312 + (time >> 2) * 1250;
		}
	}
	if (repeat_flag) {
		ptimer->timer_period = period;
	}

	uint32_t saved = LOCK_IRQ_SAVE();
	t_now = FRC2->COUNT;
	timer_insert(period + t_now, ptimer);
	LOCK_IRQ_RESTORE(saved);
}

void ICACHE_FLASH_ATTR
system_timer_reinit(void) /* Actually in user_interface.o */
{
	ets_printf("system_timer_reinit()\n");
	timer2_ms_flag = 0;
	FRC2->CTRL = 0x84; /* switch FRC2 clock from 312.5KHz (divisor 256) to 5MHz (divisor 16) */
}

/*
         U ets_intr_lock
         U ets_intr_unlock
         U ets_isr_attach
         U ets_isr_unmask
         U ets_post
         U ets_printf
         U ets_task
         U os_printf_plus
         U timer2_ms_flag
         U __udivsi3
00000000 B dbg_timer_flag
00000000 T ets_timer_setfn
00000000 t flash_str$2373_1_2
00000004 t ets_timer_handler_dsr
00000004 B timer_list
00000008 B debug_timer
0000000c B debug_timerfn
00000010 T ets_timer_disarm
00000010 T ets_timer_done
00000010 t flash_str$2375_1_3
00000010 b rtcTimerEvtQ_51
00000020 t ets_rtc_timer_arm
00000020 t ets_timer_intr_set
00000020 t flash_str$2396_3_4
00000030 t flash_str$2428_5_5
00000034 t timer_insert
00000050 t flash_str$2432_5_6
00000060 t flash_str$2433_5_7
0000006c T ets_timer_handler_isr
00000070 t flash_str$2434_5_8
00000080 t flash_str$2565_19_8
00000088 T ets_timer_arm_new
000000a0 t flash_str$2566_19_9
000000c0 t flash_str$2567_19_10
000000e0 t flash_str$2568_19_11
000000f0 t flash_str$2569_19_12
00000110 t flash_str$2570_19_13
00000114 t ets_rtc_timer_task
00000120 t flash_str$2571_19_14
00000130 t flash_str$2572_19_15
00000140 t flash_str$2573_19_16
00000150 T ets_timer_init
*/
