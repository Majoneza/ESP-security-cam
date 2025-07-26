#pragma once

/* Macro functions */
#define MAX(a, b)               \
    ({                          \
        __typeof__(a) _a = (a); \
        __typeof__(b) _b = (b); \
        _a >= _b ? _a : _b;     \
    })
#define MIN(a, b)               \
    ({                          \
        __typeof__(a) _a = (a); \
        __typeof__(b) _b = (b); \
        _a <= _b ? _a : _b;     \
    })
#define NUM_ELEMS(a) (sizeof(a) / sizeof(*a))

/* Functions */
