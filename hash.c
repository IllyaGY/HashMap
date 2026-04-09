#include "hash.h"

uint64_t hash_bytes(const void *data, size_t len) {
    if (data == NULL && len != 0)
    {
        return 0;
    }

    const unsigned char *p = data;
    uint64_t hash = 1469598103934665603ULL;

    for (size_t i = 0; i < len; i++) {
        hash ^= (uint64_t)p[i];
        hash *= 1099511628211ULL;
    }

    return hash;
}
