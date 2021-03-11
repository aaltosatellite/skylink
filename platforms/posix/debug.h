#ifndef __SKYLINK_POSIX_DEBUG_H__
#define __SKYLINK_POSIX_DEBUG_H__

#include <stdio.h>

//#define debugprintf(...) fprintf(stderr, __VA_ARGS__)
#define debugprintf(...) do { } while(0)
#define diagprintf(...) printf(__VA_ARGS__)

#endif
