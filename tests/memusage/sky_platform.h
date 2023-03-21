#ifndef __SKY_PLATFORM_H__
#define __SKY_PLATFORM_H__

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>


/* Tick is the time measurement primitive (often milliseconds), 32 bits. */
typedef int32_t tick_t;


/* */
void *instrumented_malloc(size_t n);
void instrumented_free(void* ptr);

#define SKY_MALLOC(size)    instrumented_malloc(size)
#define SKY_FREE(ptr)       instrumented_free(ptr)

/* Assert and printf for POSIX platforms */
#define SKY_PRINTF(x, ...)            \
    if ((sky_diag_mask & (x)) == (x)) \
    {                                 \
        fprintf(stderr, __VA_ARGS__); \
        fflush(stderr);               \
    }
#define SKY_ASSERT(...) assert(__VA_ARGS__);


#endif /* __SKY_PLATFORM_H__ */
