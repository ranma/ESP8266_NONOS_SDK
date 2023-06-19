#include <assert.h>
#include "c_types.h"
#include "osapi.h"

#include "relib/relib.h"
#include "relib/esp8266.h"
#include "relib/xtensa.h"
#include "relib/ets_rom.h"

#include "relib/hooks.h"

extern char jmp_hostap_deliver_data;
extern char ets_hostap_deliver_data;
extern char jmp_ieee80211_deliver_data;
extern char ets_ieee80211_deliver_data;

struct override {
	void* old_fn;
	void* new_fn;
};

static struct override overrides[] = {
	{&ets_hostap_deliver_data, &jmp_hostap_deliver_data},
	{&ets_ieee80211_deliver_data, &jmp_ieee80211_deliver_data},
};

static void
try_patch(uint32_t old, uint32_t new)
{
	uint32_t *data = (void*)old;
	int32_t delta = new-old;
	int32_t abs = delta ? delta : -delta;
	uint32_t op = ((delta - 4) << 6) | 0x6;
	uint32_t old_op;
	if ((old & 3) != 0) {
		ets_printf("not aligned\n");
		return;
	}
	old_op = *data;
	/* check for "addi a1, a1" */
	if ((old_op & 0xffff) != 0xc112) {
		ets_printf("unexpected function prologue %08x, skipped!\n", old_op);
		return;
	}
	if (old >= 0x40200000 || old < 0x40100000) {
		ets_printf("not in IRAM\n");
		return;
	}
	if (abs >= 131000) {
		ets_printf("%d out of jump range\n", delta);
		return;
	}
	/* It's a reasonably safe assumption that there'll be space in
	 * front for a literal, unless it is a very trivial function */
	*(data-1) = new;
	old_op = *data;
	*data = op;
	ets_printf("patched code %08x -> %08x\n", old_op, op);
}

void
install_iram_hooks(void)
{
	for (int i = 0; i < ARRAY_SIZE(overrides); i++) {
		ets_printf("Patching function %p -> %p\n",
			overrides[i].old_fn,
			overrides[i].new_fn);
		try_patch((uint32_t)overrides[i].old_fn, (uint32_t)overrides[i].new_fn);
	}
}

void ppTask(ETSEvent *e);
void relib_pptask(ETSEvent *e);

static void ICACHE_FLASH_ATTR
hook_task(ETSTask expected, ETSPriority prio, ETSTask new)
{
	uint32_t *ptr = (uint32_t*)0x3fffdab0;
	uint32_t current = ptr[prio * 4];
	uint32_t want = (uint32_t)expected;
	ets_printf("Hooking task %d: replace 0x%08x with %p\n",
		prio, current, new);
	if (current != want) {
		ets_printf("expected 0x%08x but got 0x%08x\n",
			want, current);
		return;
	}
	ptr[prio * 4] = (uint32_t)new;
}

void ICACHE_FLASH_ATTR
install_task_hooks(void)
{
	hook_task(ppTask, PRIO_PP, relib_pptask);
}
