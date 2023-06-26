#ifndef RELIB_PHY_OPS_H
#define RELIB_PHY_OPS_H

struct phy_ops {
	int  (*rf_init)(int ch, int n);
	int  (*set_chanfreq)(int chfr);
	void (*set_chan)(int ch);
	int  (*unset_chanfreq)(void);
	void (*enable_cca)(void);
	void (*disable_cca)(void);
	int  (*initialize_bb)(void);
	void (*set_sense)(void);
};

typedef struct phy_ops phy_ops_st;

#endif /* RELIB_PHY_OPS_H */
