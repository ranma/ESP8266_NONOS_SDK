#ifndef RELIB_OPMODE_H
#define RELIB_OPMODE_H

enum opmode {
	NULL_MODE=0,
	STATION_MODE=1,
	SOFTAP_MODE=2,
	STATIONAP_MODE=3,
} __attribute__((packed));
typedef enum opmode opmode_t;

static_assert(sizeof(opmode_t) == 1, "opmode_t size mismatch");

#endif /* RELIB_OPMODE_H */
