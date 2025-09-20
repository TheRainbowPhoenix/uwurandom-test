#ifndef _UWURANDOM_PLATFORM_H
#define _UWURANDOM_PLATFORM_H

#include "uwurandom_types.h"
#include <sys/types.h>
#include <stdlib.h>

void uwu_getrandom(void* buf, size_t len)
{
    uint8_t *bytes = buf;
    for (size_t i = 0; i < len; i++)
        *bytes++ = rand() & 0xFF;
}
#define UWU_GETRANDOM(buf, len) (uwu_getrandom((buf), (len)))

#define COPY_STR(dst, src, len) memcpy(dst,src,len)

#include <errno.h>

#include <string.h>


const size_t RAND_SIZE = 128;

static int uwu_init_rng(uwu_state* state) {
    uwu_random_number* rng_buf = malloc(RAND_SIZE * sizeof(uwu_random_number));

    if (rng_buf == NULL) {
        return -ENOMEM;
    }

    state->rng_buf = rng_buf;

    // mark the offset into rng_buf as just past the end of the buffer,
    // meaning we'll regenerate the buffer the first time we ask for random bytes
    state->rng_idx = RAND_SIZE;

    return 0;
}

static void uwu_destroy_rng(uwu_state* state) {
    if (state->rng_buf) {
        free(state->rng_buf);
        state->rng_buf = NULL;
    }
}

static uwu_random_number uwu_random_int(uwu_state* state) {
    if (state->rng_idx >= RAND_SIZE) {
        UWU_GETRANDOM(state->rng_buf, RAND_SIZE * sizeof(uwu_random_number));
        state->rng_idx = 0;
    }
    uwu_random_number rand_value = state->rng_buf[state->rng_idx];
    state->rng_idx++;
    return rand_value;
}

#define COPY_CHAR(value, dst) do {\
    *(dst) = (value);\
} while (0)

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#endif // _UWURANDOM_PLATFORM_H
