#ifndef RELIB_LMAC_CONF_MIB_H
#define RELIB_LMAC_CONF_MIB_H

struct lmac_conf_mib {
	uint32_t dot11MaxTransmitMsduLifetime;
	uint32_t dot11MaxReceiveLifetime;
	uint8_t dot11LongRetryLimit;
	uint8_t dot11ShortRetryLimit;
	uint16_t dot11RTSThreshold;
	uint16_t dot11FragmentationThreshold;
	uint16_t dot11CurrentTxPowerLevel;
	uint16_t dot11SlotTime;
	uint16_t TxPowerLevelOfdm;
	uint16_t TxPowerLevelDsssCck;
	uint16_t Eifs;
	uint16_t BasicRateTbl;
	uint16_t SupportRateMap;
	uint8_t dot11PreambleType;
	uint8_t fast_iac_more_time;
};

typedef struct lmac_conf_mib lmac_conf_mib_st;
static_assert(sizeof(lmac_conf_mib_st) == 32, "lmac_conf_mib size mismatch");

#endif /* RELIB_LMAC_CONF_MIB_H */
