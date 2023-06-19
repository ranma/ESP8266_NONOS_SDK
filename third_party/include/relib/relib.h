#ifndef RELIB_H
#define RELIB_H

#define OFFSET_OF(s, f) __builtin_offsetof(s, f)
#define ARRAY_SIZE(a) (sizeof((a)) / sizeof(*(a)))

void Cache_Read_Enable_New(void);

#endif /* RELIB_H */
