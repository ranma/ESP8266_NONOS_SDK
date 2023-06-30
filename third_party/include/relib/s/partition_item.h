#ifndef RELIB_PARTITION_ITEM_H
#define RELIB_PARTITION_ITEM_H

enum partition_type {
    SYSTEM_PARTITION_INVALID = 0,
    SYSTEM_PARTITION_BOOTLOADER,            /* user can't modify this partition address, but can modify size */
    SYSTEM_PARTITION_OTA_1,                 /* user can't modify this partition address, but can modify size */
    SYSTEM_PARTITION_OTA_2,                 /* user can't modify this partition address, but can modify size */
    SYSTEM_PARTITION_RF_CAL,                /* user must define this partition */
    SYSTEM_PARTITION_PHY_DATA,              /* user must define this partition */
    SYSTEM_PARTITION_SYSTEM_PARAMETER,      /* user must define this partition */
    SYSTEM_PARTITION_AT_PARAMETER,
    SYSTEM_PARTITION_SSL_CLIENT_CERT_PRIVKEY,
    SYSTEM_PARTITION_SSL_CLIENT_CA,
    SYSTEM_PARTITION_SSL_SERVER_CERT_PRIVKEY,
    SYSTEM_PARTITION_SSL_SERVER_CA,
    SYSTEM_PARTITION_WPA2_ENTERPRISE_CERT_PRIVKEY,
    SYSTEM_PARTITION_WPA2_ENTERPRISE_CA,
    SYSTEM_PARTITION_CUSTOMER_BEGIN = 100,  /* user can define partition after here */
    SYSTEM_PARTITION_MAX
};
typedef enum partition_type partition_type_t;

struct partition_item {
    enum partition_type type;    /* the partition type */
    uint32_t addr;            /* the partition address */
    uint32_t size;            /* the partition size */
};
typedef struct partition_item partition_item_st;
typedef struct partition_item app_partition_item_st;  /* alias name */

#endif /* RELIB_PARTITION_ITEM_H */
