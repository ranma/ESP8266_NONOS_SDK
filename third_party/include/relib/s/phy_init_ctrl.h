#ifndef RELIB_PHY_INIT_CTRL_H
#define RELIB_PHY_INIT_CTRL_H

struct esp_init_data_default {
	uint8_t _0_param_ver_id;          // 0
	uint8_t _1;
	uint8_t rx_gain_swp_step[15];  // 2
	int8_t gain_cmp[3];            // 17 / 0x11
	int8_t gain_cmp_ext2[3];       // 20 / 0x14
	int8_t gain_cmp_ext3[3];       // 23 / 0x17
	uint8_t spur_freq_cfg;         // 26 / 0x1a
	uint8_t spur_freq_cfg_div;     // 27 / 0x1b
	uint16_t spur_freq_en;         // 28 / 0x1c
	int8_t target_power_channel_offset[4];  // 30 / 0x1e
	uint8_t target_power_init[6];  // 34 / 0x22
	uint8_t ratepwr_offset[8];     // 40 / 0x28
	uint8_t xtal_freq;             // 48 / 0x30 (0: 40 MHz, 1: 26MHz, 2: 24MHz)
	uint8_t test_xtal;             // 49 / 0x31
	uint8_t sdio_conf;             // 50 / 0x32
	uint8_t dual_ant_conf;         // 51 / 0x33
	uint8_t test_uart_txd;         // 52 / 0x34
	uint8_t gpio_wake;             // 53 / 0x35
	uint8_t _54;
	uint8_t _55;
	uint8_t _56;
	uint8_t _57;
	uint8_t _58;
	uint8_t _59;
	uint8_t _60;
	uint8_t _61;
	uint8_t _62;
	uint8_t _63;
	uint8_t spur_freq_cfg_2;       // 64 / 0x40
	uint8_t spur_freq_cfg_div_2;   // 65 / 0x41
	uint16_t spur_freq_en_2;       // 66 / 0x42
	uint8_t spur_freq_cfg_msb;     // 68 / 0x44
	uint8_t spur_freq_cfg_2_msb;   // 69 / 0x45
	uint16_t spur_freq_cfg_3;      // 70 / 0x46
	uint16_t spur_freq_cfg_4;      // 72 / 0x48
	uint8_t _74;
	uint8_t _75;
	uint8_t _76;
	uint8_t _77;
	uint8_t _78;
	uint8_t _79;
	uint8_t _80;
	uint8_t _81;
	uint8_t _82;
	uint8_t _83;
	uint8_t _84;
	uint8_t _85;
	uint8_t _86;
	uint8_t _87;
	uint8_t _88;
	uint8_t _89;
	uint8_t _90;
	uint8_t _91;
	uint8_t _92;
	uint8_t low_power_en;  // 93
	uint8_t lp_rf_stg10;   // 94
	uint8_t lp_bb_att_ext; // 95
	uint8_t _96;
	uint8_t _97;
	uint8_t _98;
	uint8_t _99;
	uint8_t _100;
	uint8_t _101;
	uint8_t _102;
	uint8_t _103;
	uint8_t _104;
	uint8_t _105;
	uint8_t _106;
	uint8_t vdd33_const; // 107
	uint8_t _108;
	uint8_t ch14_en;      // 109
	uint8_t ch14_freq_lo; // 110
	uint8_t ch14_freq_hi; // 111
	uint8_t _112;
	uint8_t _113;
	uint8_t _114;
	uint8_t _115;
	uint8_t _116;
	uint8_t _117;
	uint8_t _118;
	uint8_t _119;
	uint8_t _120;
	uint8_t _121;
	uint8_t _122;
	uint8_t _123;
	uint8_t _124;
	uint8_t _125;
	uint8_t _126;
	uint8_t _127;
	uint8_t _128;
};

typedef struct esp_init_data_default esp_init_data_default_st;

static_assert(sizeof(esp_init_data_default_st) == 128, "esp_init_data_default_st size mismatch");

struct chip6_phy_init_ctrl {
	uint8_t xtal_freq; // 0 (0: 40MHz, 1: 26MHz, 2: 24MHz)
	int8_t gain_cmp[3];            // 1, chip_v6_set_chan_rx_cmp
	int8_t gain_cmp_ext2[3];       // 4, chip_v6_set_chan_rx_cmp
	int8_t gain_cmp_ext3[3];       // 7, chip_v6_set_chan_rx_cmp
	uint8_t _10;
	uint8_t spur_freq_cfg;         // 11
	uint8_t spur_freq_cfg_div;     // 12, phy_dig_spur_set
	uint8_t _13;
	uint16_t spur_freq_en;         // 14, phy_dig_spur_set
	int8_t target_power_channel_offset[4];  // 16 / 0x10
	uint8_t target_power_init[6];  // 20 / 0x14
	uint8_t ratepwr_offset[8];     // 26 / 0x1a
	uint8_t dual_ant_conf;         // 34 / 0x22, phy_gpio_cfg, ant_switch_init
	uint8_t test_uart_txd;         // 35 / 0x23, phy_gpio_cfg
	uint8_t gpio_wake;             // 36 / 0x24, phy_gpio_cfg, ant_switch_init
	uint8_t _37;
	uint8_t spur_freq_cfg_2;       // 38 / 0x26, phy_dig_spur_set
	uint8_t spur_freq_cfg_div_2;   // 39 / 0x27, phy_dig_spur_set
	uint16_t spur_freq_en_2;       // 40 / 0x28
	uint8_t spur_freq_cfg_msb;     // 42 / 0x2a, phy_dig_spur_set
	uint8_t spur_freq_cfg_2_msb;   // 43 / 0x2b
	uint16_t spur_freq_cfg_3;      // 44 / 0x2c
	uint16_t spur_freq_cfg_4;      // 46 / 0x2e, phy_dig_spur_set
	uint8_t _48; // tx_atten_set_interp
	uint8_t _49; // tx_atten_set_interp
	uint8_t _50; // tx_atten_set_interp
	uint8_t _51; // tx_atten_set_interp
	uint8_t _52; // tx_atten_set_interp
	uint8_t _53; // tx_atten_set_interp
	uint8_t _54;
	uint8_t _55;
	uint8_t _56;
	uint8_t _57; // tx_atten_set_interp; chip_v6_initialize_bb; 1 == tx_cont_cfg(1)
	uint8_t _58;
	uint8_t _59;
	uint8_t _60; // chip_v6_set_chan
	uint8_t low_power_en;  // 61; tx_atten_set_interp, 0 == set_most_pwr_reg, chip_v6_initialize_bb
	uint8_t lp_rf_stg10;   // 62; chip_v6_initialize_bb
	uint8_t lp_bb_att_ext; // 63; chip_v6_initialize_bb; low_power_set(chip6_phy_init_ctrl[61],chip6_phy_init_ctrl[62],chip6_phy_init_ctrl[63]);
	uint8_t _64;
	uint8_t _65;
	uint8_t _66;
	uint8_t _67; // periodic_cal_top
	uint8_t _68;
	uint8_t _69;
	uint8_t _70;
	uint8_t vdd33_const; // 71; pm_set_sleep_mode, txpwr_offset, vdd-related
	uint8_t _72;
	uint8_t ch14_en; // ram_set_channel_freq (1 == enable "channel 14")
	uint8_t ch14_freq_lo; // ram_set_channel_freq, pll trim
	uint8_t ch14_freq_hi; // ram_set_channel_freq, pll trim
	uint8_t _76; // chip_60_set_channel, chip_v6_set_chan_offset, phy_bb_rx_cfg
	uint8_t _77; // chip_v6_set_chan_offset
	uint8_t _78; // register_chipv6_phy
	uint8_t _79;
};
typedef struct chip6_phy_init_ctrl chip6_phy_init_ctrl_st;

static_assert(sizeof(chip6_phy_init_ctrl_st) == 80, "chip6_phy_init_ctrl_st size mismatch");

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
