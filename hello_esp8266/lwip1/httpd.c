#include "lwip/netif.h"
#include "lwip/inet.h"
#include "netif/etharp.h"
#include "lwip/tcp.h"
#include "lwip/ip.h"
#include "lwip/init.h"
#include "lwip/tcp_impl.h"
#include "lwip/memp.h"

#include "ets_sys.h"
#include "os_type.h"
#include "user_config.h"


static const char mem_debug_file[] ICACHE_RODATA_ATTR = __FILE__;

static FLASH_STR char http400_BadRequest[] =
    "HTTP/1.1 400 Bad Request\r\n"
    "Content-Type: text/html\r\n"
    "Connection: close\r\n"
    "\r\n"
    "<html><body><h1>400 Bad Request</h1></body></html>";
static FLASH_STR char http404_NotFound[] =
    "HTTP/1.1 404 Not Found\r\n"
    "Content-Type: text/html\r\n"
    "Connection: close\r\n"
    "\r\n"
    "<html><body><h1>404 Not Found</h1></body></html>";
static FLASH_STR char http302_Found[] =
    "HTTP/1.1 302 Found\r\n"
    "Content-Type: text/html\r\n"
    "Connection: close\r\n"
    "Location: http://esp.local/\r\n"
    "\r\n"
    "<html><body><h1>302 Found</h1></body></html>";
static FLASH_STR char http200_OK[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Connection: close\r\n"
    "\r\n";
static FLASH_STR char page_hello[] =
    "<html><body><h1>Hello world!</h1></body></html>";

static FLASH_STR char httpGet[4] = "GET ";
static FLASH_STR char httpProto[5] = "HTTP/";

static FLASH_STR const char tcp_state_str_rodata[][12] ICACHE_RODATA_ATTR = {
  "CLOSED",
  "LISTEN",
  "SYN_SENT",
  "SYN_RCVD",
  "ESTABLISHED",
  "FIN_WAIT_1",
  "FIN_WAIT_2",
  "CLOSE_WAIT",
  "CLOSING",
  "LAST_ACK",
  "TIME_WAIT",
};

static err_t ICACHE_FLASH_ATTR
httpd_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
	if (p == NULL) {
		os_printf("httpd_recv(arg=%p, p == NULL, err=%d) (connection closed)\n", arg, err);
		goto exit_close;
	}
	os_printf("httpd_recv(arg=%p, p->tot_len=%d, err=%d)\n", arg, p->tot_len, err);
	if (pbuf_memcmp(p, 0, httpGet, sizeof(httpGet)) != 0
	    || pbuf_memfind(p, httpProto, sizeof(httpProto), 0) == 0xFFFF) {
		tcp_write(pcb, http400_BadRequest, strlen(http400_BadRequest), TCP_WRITE_FLAG_COPY);
		goto exit_recved;
	}
	// "GET / HTTP/1.1\r\n"
	if (pbuf_get_at(p, 4) == '/' && pbuf_get_at(p, 5) == ' ') {
		tcp_write(pcb, http200_OK, strlen(http200_OK), TCP_WRITE_FLAG_COPY);
		tcp_write(pcb, page_hello, strlen(page_hello), TCP_WRITE_FLAG_COPY);
	} else {
		/* 302 instead of 404, for captive portal */
		tcp_write(pcb, http302_Found, strlen(http302_Found), TCP_WRITE_FLAG_COPY);
	}

exit_recved:
	tcp_recved(pcb, p->tot_len);
	pbuf_free(p);

exit_close:
	if (arg) {
		tcp_arg(pcb, NULL);
		os_free(arg);
	}
	tcp_close(pcb);
}

static void ICACHE_FLASH_ATTR
httpd_error(void *arg, err_t err)
{
	os_printf("httpd_error(err=%d)\n", err);
}

static err_t ICACHE_FLASH_ATTR
httpd_poll(void *arg, struct tcp_pcb *pcb)
{
	os_printf("httpd_poll(pcb=%p, pcb->state=%s)\n",
		pcb, tcp_state_str_rodata[pcb->state]);
	return ERR_OK;
}

static err_t ICACHE_FLASH_ATTR
httpd_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
	tcp_nagle_disable(newpcb);

	os_printf("httpd_accept(arg=%p, err=%d)\n", arg, err);
	/* tcp_arg(newpcb, conn_info); */

	/* initialize LwIP tcp_recv callback function for newpcb */
	tcp_recv(newpcb, httpd_recv);
	/* initialize LwIP tcp_err callback function for newpcb */
	tcp_err(newpcb, httpd_error);
	/* initialize LwIP tcp_poll callback function for newpcb */
	tcp_poll(newpcb, httpd_poll, 16); /* 4 times TCP coarse timer */
	return ERR_OK;
}

static struct tcp_pcb *server_pcb;
static struct tcp_pcb *listen_pcb;

int ICACHE_FLASH_ATTR
httpd_init(uint32_t ip_addr, int port)
{
	ip_addr_t lwip_addr = {ip_addr};

	server_pcb = tcp_new();
	if (server_pcb == NULL) {
		return -1;
	}

	err_t err = tcp_bind(server_pcb, &lwip_addr, port);
	if (err != ERR_OK) {
		memp_free(MEMP_TCP_PCB, server_pcb);
		return -2;
	}

	// start listening
	listen_pcb = tcp_listen(server_pcb);
	if (listen_pcb == NULL) {
		memp_free(MEMP_TCP_PCB, server_pcb);
		return -3;
	}

	/* set arg passed to callback
	tcp_arg(lcpb, myptr);
	*/

	// set accept callback
	tcp_accept(listen_pcb, httpd_accept);

	return 0;
}
