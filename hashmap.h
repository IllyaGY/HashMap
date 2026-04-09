#pragma once

#include <stddef.h>

typedef struct hashmap hashmap;
typedef void (*hm_iter_fn)(
    size_t bucket_index,
    size_t chain_index,
    const void *key,
    size_t key_len,
    const void *value,
    size_t value_len,
    void *ctx
);

int create_hm(hashmap **map, size_t initial_capacity);
int delete_hm(hashmap **map);

int insert_hm(hashmap *map, const void *key, size_t key_len, const void *value, size_t value_len);
int get_hm(const hashmap *map, const void *key, size_t key_len, const void **value, size_t *value_len);
int remove_hm(hashmap *map, const void *key, size_t key_len);
int contains_hm(const hashmap *map, const void *key, size_t key_len, int *result);
int foreach_hm(const hashmap *map, hm_iter_fn func, void *ctx);

int is_empty_hm(const hashmap *map, int *result);
int get_size_hm(const hashmap *map, size_t *size);
int get_capacity_hm(const hashmap *map, size_t *capacity);
