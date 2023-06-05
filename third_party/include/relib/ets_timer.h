void timer_insert(uint32_t timer_expire, ETSTimer *ptimer);
void ets_timer_intr_set(uint32_t timer_expire);
void ets_rtc_timer_arm(uint32_t timer_expire);
void ets_timer_handler_isr(void);
void ets_timer_init(void);

void ets_timer_arm_new(ETSTimer *ptimer, uint32_t time, bool repeat_flag, bool ms_flag);
void ets_timer_disarm(ETSTimer *ptimer);
void ets_timer_setfn(ETSTimer *ptimer, ETSTimerFunc *pfunction, void *parg);
