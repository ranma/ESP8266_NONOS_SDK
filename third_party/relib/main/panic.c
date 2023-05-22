/*
 ROM function, ets_uart_printf(), prints on the UART selected by
 uart_buff_switch(). Supported format options are the same as vprintf(). Also
 has cooked newline behavior. No flash format/string support; however, ISR safe.
 It also uses a static function in ROM to print characters. The UART selection
 is handled by a prior call to uart_buff_switch(). An advantage over ets_printf,
 this call is not affected by calls made to ets_install_putc1 or
 ets_install_putc2.
 */
extern int ets_uart_printf(const char *format, ...) __attribute__ ((format (printf, 1, 2)));

void
panic_handler(int n, int pc, int depc)
{
	ets_uart_printf("\nPanic %d pc=%08x DEPC=%08x\n", n, pc, depc);
	while(1);
}
