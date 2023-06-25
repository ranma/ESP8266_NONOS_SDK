#ifndef RELIB_PHY_INIT_CTRL_H
#define RELIB_PHY_INIT_CTRL_H

struct phy_init_ctrl {
	/* shallow struct def to avoid pulling in all the other struct members */
	union {
		struct {
			uint8_t param_ver_id;
			uint8_t pad[107];
		}
	};
};

typedef struct phy_init_ctrl phy_init_ctrl_st;

static_assert(sizeof(phy_init_ctrl_st) == 108, "phy_init_ctrl size mismatch");

#endif /* RELIB_PHY_INIT_CTRL_H */
