#ifndef RELIB_WDEV_STATUS_H
#define RELIB_WDEV_STATUS_H

struct wdev_status {
    uint32_t tx_enable_counts[12];
    uint32_t tx_queue_counts[12];
    uint32_t tx_collision_count[12];
    uint32_t tx_suc_count[12];
    uint32_t tx_cts_timeout_count[12];
    uint32_t tx_ack_timeout_count[12];
    uint32_t tx_data_err;
    uint32_t rx_suc_data_count;
    uint32_t rx_ampdu_count;
    uint32_t rx_bar_count;
    uint32_t rx_ba_count;
    uint32_t rx_pspoll_count;
    uint32_t rx_other_control_count;
    uint32_t rx_my_management_count;
    uint32_t rx_my_data_count;
    uint32_t rx_multicast_count;
    uint32_t rx_bcn_count;
    uint32_t rx_type_error_count;
    uint32_t rx_pool_error_count;
    uint32_t rx_hung_count;
    uint32_t panic_reset_count;
    uint32_t dirty_rxbuf_count;
};

typedef struct wdev_status wdev_status_st;

#endif /* RELIB_WDEV_STATUS_H */
