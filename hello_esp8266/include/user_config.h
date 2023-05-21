#define DNS_SERVER_PORT  53
#define HTTP_SERVER_PORT 80

#define FLASH_STR const ICACHE_RODATA_ATTR STORE_ATTR

extern int httpd_init(uint32_t ip_addr, int port);
extern int cdnsd_init(uint32_t ip_addr, int port);
