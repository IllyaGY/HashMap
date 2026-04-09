#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "hash.h"
#include "hashmap.h"
#include "hashmap_log.h"

#define HM_DEFAULT_CAPACITY 8U
#define HM_MAX_LOAD_NUM 3U
#define HM_MAX_LOAD_DEN 4U

typedef struct hash_node {
    uint64_t hash;
    void *key;
    size_t key_len;
    void *value;
    size_t value_len;
    struct hash_node *next;
} hash_node;

struct hashmap {
    hash_node **buckets;
    size_t count;
    size_t capacity;
};

static int allocate_bytes(void **dest, const void *src, size_t len){
    if (dest == NULL)
    {
        CB_LOG_ERROR("The destination pointer is NULL");
        return EFAULT;
    }
    if (len == 0)
    {
        *dest = NULL;
        return 0;
    }
    if (src == NULL)
    {
        CB_LOG_ERROR("A non-empty byte range requires a non-NULL source pointer");
        return EFAULT;
    }

    void *copy = malloc(len);
    if (copy == NULL)
    {
        CB_LOG_ERROR("Failed to allocate memory for byte storage");
        return ENOMEM;
    }

    memcpy(copy, src, len);
    *dest = copy;
    return 0;
}

static int create_node(
    hash_node **node_ptr,
    uint64_t hash,
    const void *key,
    size_t key_len,
    const void *value,
    size_t value_len
){
    if (node_ptr == NULL)
    {
        CB_LOG_ERROR("The destination node pointer is NULL");
        return EFAULT;
    }

    hash_node *node = calloc(1, sizeof(*node));
    if (node == NULL)
    {
        CB_LOG_ERROR("Failed to allocate memory for a hash node");
        return ENOMEM;
    }

    int err = allocate_bytes(&node->key, key, key_len);
    if (err != 0)
    {
        free(node);
        return err;
    }

    err = allocate_bytes(&node->value, value, value_len);
    if (err != 0)
    {
        free(node->key);
        free(node);
        return err;
    }

    node->hash = hash;
    node->key_len = key_len;
    node->value_len = value_len;
    node->next = NULL;

    *node_ptr = node;
    return 0;
}

static void destroy_node(hash_node *node){
    if (node == NULL)
    {
        return;
    }

    free(node->key);
    free(node->value);
    free(node);
}

static int key_matches(const hash_node *node, uint64_t hash, const void *key, size_t key_len){
    if (node->hash != hash || node->key_len != key_len)
    {
        return 0;
    }
    if (key_len == 0)
    {
        return 1;
    }

    return memcmp(node->key, key, key_len) == 0;
}

static size_t bucket_index(uint64_t hash, size_t capacity){
    return (size_t)(hash % capacity);
}

static int rehash_hm(hashmap *map, size_t new_capacity){
    if (map == NULL)
    {
        CB_LOG_ERROR("The hashmap pointer is NULL");
        return EFAULT;
    }
    if (new_capacity == 0)
    {
        CB_LOG_ERROR("The new capacity must be greater than zero");
        return EINVAL;
    }

    hash_node **new_buckets = calloc(new_capacity, sizeof(*new_buckets));
    if (new_buckets == NULL)
    {
        CB_LOG_ERROR("Failed to allocate memory for resized bucket storage");
        return ENOMEM;
    }

    for (size_t i = 0; i < map->capacity; i++) {
        hash_node *node = map->buckets[i];
        while (node != NULL) {
            hash_node *next = node->next;
            size_t index = bucket_index(node->hash, new_capacity);
            node->next = new_buckets[index];
            new_buckets[index] = node;
            node = next;
        }
    }

    free(map->buckets);
    map->buckets = new_buckets;
    map->capacity = new_capacity;
    return 0;
}

static int ensure_capacity_for_insert(hashmap *map){
    if (map == NULL)
    {
        CB_LOG_ERROR("The hashmap pointer is NULL");
        return EFAULT;
    }
    if ((map->count + 1) * HM_MAX_LOAD_DEN <= map->capacity * HM_MAX_LOAD_NUM)
    {
        return 0;
    }
    if (map->capacity > SIZE_MAX / 2)
    {
        CB_LOG_ERROR("The hashmap cannot grow any further");
        return EOVERFLOW;
    }

    return rehash_hm(map, map->capacity * 2);
}

int create_hm(hashmap **map, size_t initial_capacity){
    if (map == NULL)
    {
        CB_LOG_ERROR("The provided pointer to the hashmap pointer is NULL");
        return EFAULT;
    }

    size_t capacity = initial_capacity == 0 ? HM_DEFAULT_CAPACITY : initial_capacity;

    hashmap *created = malloc(sizeof(*created));
    if (created == NULL)
    {
        CB_LOG_ERROR("Failed to allocate memory for hashmap");
        return ENOMEM;
    }

    created->buckets = calloc(capacity, sizeof(*created->buckets));
    if (created->buckets == NULL)
    {
        CB_LOG_ERROR("Failed to allocate memory for bucket storage");
        free(created);
        return ENOMEM;
    }

    created->count = 0;
    created->capacity = capacity;
    *map = created;
    return 0;
}

int delete_hm(hashmap **map){
    if (map == NULL)
    {
        CB_LOG_ERROR("The pointer to the hashmap pointer is NULL");
        return EFAULT;
    }
    if (*map == NULL)
    {
        CB_LOG_ERROR("The hashmap pointer is NULL");
        return EFAULT;
    }

    for (size_t i = 0; i < (*map)->capacity; i++) {
        hash_node *node = (*map)->buckets[i];
        while (node != NULL) {
            hash_node *next = node->next;
            destroy_node(node);
            node = next;
        }
    }

    free((*map)->buckets);
    free(*map);
    *map = NULL;
    return 0;
}

int insert_hm(
    hashmap *map,
    const void *key,
    size_t key_len,
    const void *value,
    size_t value_len
){
    if (map == NULL)
    {
        CB_LOG_ERROR("The hashmap pointer is NULL");
        return EFAULT;
    }
    if (key == NULL && key_len != 0)
    {
        CB_LOG_ERROR("The key pointer is NULL for a non-empty key");
        return EFAULT;
    }
    if (value == NULL && value_len != 0)
    {
        CB_LOG_ERROR("The value pointer is NULL for a non-empty value");
        return EFAULT;
    }

    int err = ensure_capacity_for_insert(map);
    if (err != 0)
    {
        return err;
    }

    uint64_t hash = hash_bytes(key, key_len);
    size_t index = bucket_index(hash, map->capacity);

    for (hash_node *node = map->buckets[index]; node != NULL; node = node->next) {
        if (!key_matches(node, hash, key, key_len))
        {
            continue;
        }

        void *new_value = NULL;
        err = allocate_bytes(&new_value, value, value_len);
        if (err != 0)
        {
            return err;
        }

        free(node->value);
        node->value = new_value;
        node->value_len = value_len;
        return 0;
    }

    hash_node *node = NULL;
    err = create_node(&node, hash, key, key_len, value, value_len);
    if (err != 0)
    {
        return err;
    }

    node->next = map->buckets[index];
    map->buckets[index] = node;
    map->count++;
    return 0;
}

int get_hm(
    const hashmap *map,
    const void *key,
    size_t key_len,
    const void **value,
    size_t *value_len
){
    if (map == NULL)
    {
        CB_LOG_ERROR("The hashmap pointer is NULL");
        return EFAULT;
    }
    if (key == NULL && key_len != 0)
    {
        CB_LOG_ERROR("The key pointer is NULL for a non-empty key");
        return EFAULT;
    }
    if (value == NULL)
    {
        CB_LOG_ERROR("The output value pointer is NULL");
        return EFAULT;
    }
    if (value_len == NULL)
    {
        CB_LOG_ERROR("The output value length pointer is NULL");
        return EFAULT;
    }

    uint64_t hash = hash_bytes(key, key_len);
    size_t index = bucket_index(hash, map->capacity);

    for (hash_node *node = map->buckets[index]; node != NULL; node = node->next) {
        if (key_matches(node, hash, key, key_len))
        {
            *value = node->value;
            *value_len = node->value_len;
            return 0;
        }
    }

    return ENOENT;
}

int remove_hm(hashmap *map, const void *key, size_t key_len){
    if (map == NULL)
    {
        CB_LOG_ERROR("The hashmap pointer is NULL");
        return EFAULT;
    }
    if (key == NULL && key_len != 0)
    {
        CB_LOG_ERROR("The key pointer is NULL for a non-empty key");
        return EFAULT;
    }

    uint64_t hash = hash_bytes(key, key_len);
    size_t index = bucket_index(hash, map->capacity);
    hash_node *prev = NULL;
    hash_node *node = map->buckets[index];

    while (node != NULL) {
        if (key_matches(node, hash, key, key_len))
        {
            if (prev == NULL)
            {
                map->buckets[index] = node->next;
            }
            else
            {
                prev->next = node->next;
            }

            destroy_node(node);
            map->count--;
            return 0;
        }

        prev = node;
        node = node->next;
    }

    return ENOENT;
}

int contains_hm(const hashmap *map, const void *key, size_t key_len, int *result){
    if (result == NULL)
    {
        CB_LOG_ERROR("The output result pointer is NULL");
        return EFAULT;
    }

    const void *value = NULL;
    size_t value_len = 0;
    int err = get_hm(map, key, key_len, &value, &value_len);
    if (err == 0)
    {
        *result = 1;
        return 0;
    }
    if (err == ENOENT)
    {
        *result = 0;
        return 0;
    }

    return err;
}

int foreach_hm(const hashmap *map, hm_iter_fn func, void *ctx){
    if (map == NULL)
    {
        CB_LOG_ERROR("The hashmap pointer is NULL");
        return EFAULT;
    }
    if (func == NULL)
    {
        CB_LOG_ERROR("The iterator callback pointer is NULL");
        return EFAULT;
    }

    for (size_t i = 0; i < map->capacity; i++) {
        size_t chain_index = 0;
        for (hash_node *node = map->buckets[i]; node != NULL; node = node->next) {
            func(
                i,
                chain_index,
                node->key,
                node->key_len,
                node->value,
                node->value_len,
                ctx
            );
            chain_index++;
        }
    }

    return 0;
}

int is_empty_hm(const hashmap *map, int *result){
    if (map == NULL)
    {
        CB_LOG_ERROR("The hashmap pointer is NULL");
        return EFAULT;
    }
    if (result == NULL)
    {
        CB_LOG_ERROR("The output result pointer is NULL");
        return EFAULT;
    }

    *result = map->count == 0;
    return 0;
}

int get_size_hm(const hashmap *map, size_t *size){
    if (map == NULL)
    {
        CB_LOG_ERROR("The hashmap pointer is NULL");
        return EFAULT;
    }
    if (size == NULL)
    {
        CB_LOG_ERROR("The output size pointer is NULL");
        return EFAULT;
    }

    *size = map->count;
    return 0;
}

int get_capacity_hm(const hashmap *map, size_t *capacity){
    if (map == NULL)
    {
        CB_LOG_ERROR("The hashmap pointer is NULL");
        return EFAULT;
    }
    if (capacity == NULL)
    {
        CB_LOG_ERROR("The output capacity pointer is NULL");
        return EFAULT;
    }

    *capacity = map->capacity;
    return 0;
}
