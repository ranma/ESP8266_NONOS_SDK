#ifndef RELIB_H
#define RELIB_H

#define OFFSET_OF(s, f) __builtin_offsetof(s, f)
#define ARRAY_SIZE(a) (sizeof((a)) / sizeof(*(a)))

/* Check if t1 is after t2 */
#define TIME_AFTER(t1, t2) ((int)(t1 - t2) > 0)

void Cache_Read_Enable_New(void);

#endif /* RELIB_H */
