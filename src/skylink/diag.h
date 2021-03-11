#ifndef __SKYLINK_DIAG_H__
#define __SKYLINK_DIAG_H__

/*
 * Diagnostics and debug tools
 */

// TODO: Defines which prints are enabled

#define SKY_DIAG_INFO       0x0001
#define SKY_DIAG_DEBUG      0x0002
#define SKY_DIAG_BUG        0x0004
#define SKY_DIAG_LINK_STATE 0x0004

#ifdef DEBUG
#define SKY_ASSERT(x)    if (x) while(1);
#else
#define SKY_ASSERT(...)   do { } while(0)
#endif

#if 0

#define SKY_PRINTF(x, ...) fprintf(stderr, __VA_ARGS__)

#else

#define SKY_PRINTF(...) do { } while(0)

#endif



#endif /* __SKYLINK_DIAG_H__ */
