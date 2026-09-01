#include "font_route_cache.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
        uint64_t hash;
        XtpFontRouteKey key;
        XtpFontRouteValue value;
        int hash_next;
        int lru_previous;
        int lru_next;
} XtpFontRouteCacheEntry;

struct XtpFontRouteCache
{
        XtpFontRouteCacheEntry *entries;
        int *buckets;
        size_t capacity;
        size_t bucket_count;
        size_t count;
        int lru_head;
        int lru_tail;
};

static uint64_t
HashBytes(uint64_t hash, const void *bytes, size_t length)
{
        const unsigned char *next = bytes;
        size_t index;

        for (index = 0; index < length; ++index) {
                hash ^= next[index];
                hash *= UINT64_C(1099511628211);
        }
        return hash;
}

static uint64_t
HashKey(const XtpFontRouteKey *key)
{
        uint64_t hash = UINT64_C(1469598103934665603);

        hash = HashBytes(hash, key->text, key->text_length);
        hash = HashBytes(hash, &key->text_length, sizeof(key->text_length));
        hash = HashBytes(hash, &key->width, sizeof(key->width));
        hash = HashBytes(hash, &key->presentation, sizeof(key->presentation));
        hash = HashBytes(hash, &key->presentation_policy, sizeof(key->presentation_policy));
        hash = HashBytes(hash, &key->slot, sizeof(key->slot));
        hash = HashBytes(hash, &key->capturing_slot, sizeof(key->capturing_slot));
        hash = HashBytes(hash, &key->color_glyphs, sizeof(key->color_glyphs));
        hash = HashBytes(hash, &key->system_fallback, sizeof(key->system_fallback));
        return HashBytes(hash, &key->generation, sizeof(key->generation));
}

bool
XtpFontRouteKeysEqual(const XtpFontRouteKey *left, const XtpFontRouteKey *right)
{
        if (left == NULL || right == NULL)
                return false;
        return left->text_length == right->text_length && left->width == right->width &&
               left->presentation == right->presentation &&
               left->presentation_policy == right->presentation_policy &&
               left->slot == right->slot && left->capturing_slot == right->capturing_slot &&
               left->color_glyphs == right->color_glyphs &&
               left->system_fallback == right->system_fallback &&
               left->generation == right->generation &&
               memcmp(left->text, right->text, left->text_length) == 0;
}

static void
UnlinkLru(XtpFontRouteCache *cache, int index)
{
        XtpFontRouteCacheEntry *entry = &cache->entries[index];

        if (entry->lru_previous >= 0)
                cache->entries[entry->lru_previous].lru_next = entry->lru_next;
        else
                cache->lru_head = entry->lru_next;
        if (entry->lru_next >= 0)
                cache->entries[entry->lru_next].lru_previous = entry->lru_previous;
        else
                cache->lru_tail = entry->lru_previous;
}

static void
LinkLruHead(XtpFontRouteCache *cache, int index)
{
        XtpFontRouteCacheEntry *entry = &cache->entries[index];

        entry->lru_previous = -1;
        entry->lru_next = cache->lru_head;
        if (cache->lru_head >= 0)
                cache->entries[cache->lru_head].lru_previous = index;
        else
                cache->lru_tail = index;
        cache->lru_head = index;
}

static void
Touch(XtpFontRouteCache *cache, int index)
{
        if (cache->lru_head == index)
                return;
        UnlinkLru(cache, index);
        LinkLruHead(cache, index);
}

static void
RemoveFromBucket(XtpFontRouteCache *cache, int index)
{
        XtpFontRouteCacheEntry *entry = &cache->entries[index];
        size_t bucket = (size_t)(entry->hash & (cache->bucket_count - 1U));
        int *link = &cache->buckets[bucket];

        while (*link >= 0) {
                if (*link == index) {
                        *link = entry->hash_next;
                        return;
                }
                link = &cache->entries[*link].hash_next;
        }
}

XtpFontRouteCache *
XtpFontRouteCacheCreate(size_t capacity)
{
        XtpFontRouteCache *cache;
        size_t bucket_count = 1;
        size_t index;

        if (capacity == 0 || capacity > (size_t)INT_MAX || capacity > SIZE_MAX / 2U)
                return NULL;
        while (bucket_count < capacity * 2U) {
                if (bucket_count > SIZE_MAX / 2U)
                        return NULL;
                bucket_count *= 2U;
        }
        cache = calloc(1, sizeof(*cache));
        if (cache == NULL)
                return NULL;
        cache->entries = calloc(capacity, sizeof(*cache->entries));
        cache->buckets = malloc(bucket_count * sizeof(*cache->buckets));
        if (cache->entries == NULL || cache->buckets == NULL) {
                XtpFontRouteCacheDestroy(cache);
                return NULL;
        }
        for (index = 0; index < bucket_count; ++index)
                cache->buckets[index] = -1;
        cache->capacity = capacity;
        cache->bucket_count = bucket_count;
        cache->lru_head = -1;
        cache->lru_tail = -1;
        return cache;
}

void
XtpFontRouteCacheDestroy(XtpFontRouteCache *cache)
{
        if (cache == NULL)
                return;
        free(cache->entries);
        free(cache->buckets);
        free(cache);
}

bool
XtpFontRouteCacheLookup(XtpFontRouteCache *cache, const XtpFontRouteKey *key,
                        XtpFontRouteValue *value)
{
        uint64_t hash;
        int index;

        if (cache == NULL || key == NULL || key->text_length >= XTP_FONT_ROUTE_TEXT_CAPACITY)
                return false;
        hash = HashKey(key);
        index = cache->buckets[hash & (cache->bucket_count - 1U)];
        while (index >= 0) {
                XtpFontRouteCacheEntry *entry = &cache->entries[index];

                if (entry->hash == hash && XtpFontRouteKeysEqual(&entry->key, key)) {
                        if (value != NULL)
                                *value = entry->value;
                        Touch(cache, index);
                        return true;
                }
                index = entry->hash_next;
        }
        return false;
}

bool
XtpFontRouteCacheStore(XtpFontRouteCache *cache, const XtpFontRouteKey *key,
                       XtpFontRouteValue value)
{
        uint64_t hash;
        size_t bucket;
        int index;

        if (cache == NULL || key == NULL || key->text_length >= XTP_FONT_ROUTE_TEXT_CAPACITY)
                return false;
        hash = HashKey(key);
        bucket = (size_t)(hash & (cache->bucket_count - 1U));
        index = cache->buckets[bucket];
        while (index >= 0) {
                XtpFontRouteCacheEntry *entry = &cache->entries[index];

                if (entry->hash == hash && XtpFontRouteKeysEqual(&entry->key, key)) {
                        entry->value = value;
                        Touch(cache, index);
                        return true;
                }
                index = entry->hash_next;
        }
        if (cache->count < cache->capacity) {
                index = (int)cache->count++;
        } else {
                index = cache->lru_tail;
                RemoveFromBucket(cache, index);
                UnlinkLru(cache, index);
        }
        cache->entries[index].hash = hash;
        cache->entries[index].key = *key;
        cache->entries[index].value = value;
        cache->entries[index].hash_next = cache->buckets[bucket];
        cache->buckets[bucket] = index;
        LinkLruHead(cache, index);
        return true;
}

size_t
XtpFontRouteCacheCount(const XtpFontRouteCache *cache)
{
        return cache != NULL ? cache->count : 0;
}
