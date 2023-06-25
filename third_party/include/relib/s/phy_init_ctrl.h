#ifndef RELIB_PHY_INIT_CTRL_H
#define RELIB_PHY_INIT_CTRL_H

struct phy_init_ctrl {
	uint8_t param_ver_id;
	uint8_t crystal_select;
	uint8_t u0txd_disable;
	int8_t rx_gain_swp_step[15];
	int8_t gain_cmp[3];
	int8_t gain_cmp_ext2[3];
	int8_t gain_cmp_ext3[3];
	uint8_t gain_max;
	uint8_t spur_freq_cfg;
	uint8_t spur_freq_cfg_div;
	uint16_t spur_freq_en;
	int8_t target_power_channel_offset[4];
	uint8_t target_power_init[6];
	uint8_t ratepwr_offset[8];
	uint8_t xtal_freq;
	uint8_t test_xtal;
	uint8_t sdio_conf;
	uint8_t bt_conf;
	uint8_t bt_protocol;
	uint8_t dual_ant_conf;
	uint8_t test_uart_txd;
	uint8_t share_xtal;
	uint8_t gpio_wake;
	uint8_t spur_freq_cfg_2;
	uint8_t spur_freq_cfg_div_2;
	uint16_t spur_freq_en_2;
	uint8_t spur_freq_cfg_msb;
	uint8_t spur_freq_cfg_2_msb;
	uint16_t spur_freq_cfg_3;
	uint16_t spur_freq_cfg_4;
	uint8_t bin_version;
	uint8_t fcc_pwr_max_h;
	uint8_t fcc_pwr_max_l;
	uint8_t fcc_chan_num_h;
	uint8_t fcc_chan_num_l;
	uint8_t fcc_chan_interp_h;
	uint8_t fcc_chan_interp_l;
	uint8_t fcc_11b_offset_h;
	uint8_t fcc_11b_offset_l;
	uint8_t fcc_enable;
	uint8_t app_test_flg;
	uint8_t app_tx_en;
	uint8_t app_fcc_en;
	uint8_t app_tx_chn;
	uint8_t app_tx_rateOffset;
	uint8_t rd_reg_addr0;
	uint8_t rd_reg_addr1;
	uint8_t rd_reg_addr2;
	uint8_t rd_reg_addr3;
	uint8_t rd_reg_test_en;
	uint8_t app_tx_dut;
	uint8_t app_tx_tone;
	int8_t app_tone_offset_khz;
	uint8_t dpd_enable;
	uint8_t low_power_en;
	uint8_t lp_rf_stg10;
	uint8_t lp_bb_att_ext;
	uint8_t pwr_ind_11b_en;
	uint8_t pwr_ind_11b[2];
	uint8_t txpwr_track_disable;
	uint8_t sleep_change_pll;
	uint8_t wr_reg_data0;
	uint8_t wr_reg_data1;
	uint8_t wr_reg_data2;
	uint8_t wr_reg_data3;
	uint8_t periodic_cal_time;
	uint8_t do_pwctrl_time;
	uint8_t vdd33_const;
} __attribute__((packed));

typedef struct phy_init_ctrl phy_init_ctrl_st;

static_assert(sizeof(phy_init_ctrl_st) == 108, "phy_init_ctrl size mismatch");

#endif /* RELIB_PHY_INIT_CTRL_H */
