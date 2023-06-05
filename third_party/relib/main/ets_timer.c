#include <stdint.h>

#include "c_types.h"
#include "osapi.h"

#include "relib/esp8266.h"
#include "relib/ets_rom.h"
#include "relib/ets_timer.h"

#define ARRAY_SIZE(a) (sizeof((a)) / sizeof(*(a)))

#define LOCK_IRQ_SAVE() ({ \
	uint32_t ps; \
	__asm__ __volatile__("rsil %0, 3\n" : "=a"(ps) ::); \
	ps; \
})

#define LOCK_IRQ_RESTORE(c) do { \
	__asm__ __volatile__("wsr %0, PS\n" :: "a"(c) :); \
} while (0)

static ETSEvent rtcTimerEvtQ[4];

ETSTimer* timer_list;  /* fpm_onEtsIdle and pm_onEtsIdle reference this directly! */
uint8_t timer2_ms_flag;

void
ets_rtc_timer_arm(uint32_t timer_expire)
{
	int t_now = FRC2->COUNT;
	if ((int)(timer_expire - t_now) < 1) {
		/* already expired, schedule it shortly into the future */
		timer_expire = t_now + 0x50;
	}
	FRC2->ALARM = timer_expire;
}

void
ets_timer_intr_set(uint32_t timer_expire)
{
	int slop;
	int t_now;
	int t_now_with_slop;

	t_now = FRC2->COUNT;
	slop = 1280;
	if (timer2_ms_flag != '\0') {
		slop = 80;
	}
	t_now_with_slop = t_now + slop;
	if ((int)(timer_expire - t_now_with_slop) < 1) {
		/* expiring shortly or already expired */
		if ((int)(timer_expire - t_now) < 1) {
			/* already expired, schedule shortly into the future */
			ets_rtc_timer_arm(t_now_with_slop);
		}
		else {
			/* expiring soon, schedule with slop and a bit extra */
			ets_rtc_timer_arm(timer_expire + slop + 0x40);
		}
	}
	else {
		/* far enough into the future, schedule as-is */
		ets_rtc_timer_arm(timer_expire);
	}
}

/* Check if t1 is after t2 */
#define TIME_AFTER(t1, t2) ((t1 - t2) > 0)

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
		ets_timer_intr_set(timer_expire);
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
	now_us = WDEV_TIMER->COUNT;
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
			ets_timer_intr_set(timer_list->timer_expire);
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
			if ((uint32_t)(WDEV_TIMER->COUNT - now_us) < 15001) {
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
	FRC2->CTRL = 0x88;
	FRC2->LOAD = 0;
}

void
ets_timer_disarm(ETSTimer *ptimer)
{
	uint32_t saved = LOCK_IRQ_SAVE();
	if (timer_list == ptimer) {
		/* ptimer is the pending timer, update FRC2 alarm */
		timer_list = ptimer->timer_next;
		ets_timer_intr_set(timer_list->timer_expire);
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
