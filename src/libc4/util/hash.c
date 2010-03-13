/*
 * C4Hash is a hash table implementation based on the apr_hash code from
 * APR, with a few custom features added. This version is taken from APR
 * trunk, as of July 3 11:00 AM PST (r791000). ~nrc
 */

/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <string.h>

#include "c4-internal.h"
#include "util/hash.h"

/*
 * The internal form of a hash table.
 *
 * The table is an array indexed by the hash of the key; collisions
 * are resolved by hanging a linked list of hash entries off each
 * element of the array. Although this is a really simple design it
 * isn't too bad given that pools have a low allocation overhead.
 */

typedef struct c4_hash_entry_t c4_hash_entry_t;

struct c4_hash_entry_t {
    c4_hash_entry_t *next;
    unsigned int     hash;
    const void      *key;
    void            *val;
};

/*
 * Data structure for iterating through a hash table.
 *
 * We keep a pointer to the next hash entry here to allow the current
 * hash entry to be freed or otherwise mangled between calls to
 * c4_hash_iter_next().
 */
struct c4_hash_index_t {
    c4_hash_t         *ht;
    c4_hash_entry_t   *this, *next;
    unsigned int        index;
    bool                at_end;
};

/*
 * The size of the array is always a power of two. We use the maximum index
 * rather than the size so that we can use bitwise-AND for modular
 * arithmetic. The count of hash entries may be greater depending on the
 * chosen collision rate.
 *
 * We allocate the bucket array in a sub-pool, "array_pool". This allows us
 * to reclaim the old bucket array after an expansion.
 */
struct c4_hash_t {
    apr_pool_t          *pool;
    apr_pool_t          *array_pool;
    c4_hash_entry_t   **array;
    unsigned int        count, max;
    int                 key_len;
    void               *user_data;
    c4_hashfunc_t       hash_func;
    c4_keycomp_func_t   cmp_func;
    c4_hash_entry_t    *free;  /* List of recycled entries */
};

#define INITIAL_MAX 15 /* tunable == 2^n - 1 */


/*
 * Hash creation functions.
 */

static c4_hash_entry_t **alloc_array(c4_hash_t *ht, unsigned int max)
{
   return apr_pcalloc(ht->array_pool, sizeof(*ht->array) * (max + 1));
}

c4_hash_t *c4_hash_make(apr_pool_t *pool, int key_len, void *cb_data,
                        c4_hashfunc_t hash_func, c4_keycomp_func_t cmp_func)
{
    apr_pool_t *array_pool;
    c4_hash_t *ht;

    if (apr_pool_create(&array_pool, pool) != APR_SUCCESS)
        return NULL;

    ht = apr_palloc(pool, sizeof(c4_hash_t));
    ht->pool = pool;
    ht->array_pool = array_pool;
    ht->free = NULL;
    ht->count = 0;
    ht->max = INITIAL_MAX;
    ht->array = alloc_array(ht, ht->max);
    ht->key_len = key_len;
    ht->user_data = cb_data;
    ht->hash_func = hash_func;
    ht->cmp_func = cmp_func;
    return ht;
}

/*
 * Hash iteration functions.
 */
c4_hash_index_t *c4_hash_iter_make(apr_pool_t *p, c4_hash_t *ht)
{
    c4_hash_index_t *hi;

    hi = apr_palloc(p, sizeof(*hi));
    hi->ht = ht;
    c4_hash_iter_reset(hi);

    return hi;
}

void c4_hash_iter_reset(c4_hash_index_t *hi)
{
    hi->index = 0;
    hi->this = NULL;
    hi->next = NULL;
    hi->at_end = false;
}

bool c4_hash_iter_next(c4_hash_index_t *hi)
{
    if (hi->at_end)
        return false;

    hi->this = hi->next;
    while (!hi->this) {
        if (hi->index > hi->ht->max) {
            hi->at_end = true;
            return false;
        }

        hi->this = hi->ht->array[hi->index++];
    }
    hi->next = hi->this->next;
    /*
     * GCC perf hack: We are very likely to read the next entry in the hash
     * table in the near future. Hence, prefetch to try to avoid a cache
     * miss. It seems to even be a win to do the conditional branch to prefetch
     * the next bucket as well.
     */
    __builtin_prefetch(hi->next);
    if (hi->index <= hi->ht->max)
        __builtin_prefetch(hi->ht->array[hi->index]);
    return true;
}

void *c4_hash_this(c4_hash_index_t *hi, const void **key)
{
    if (hi->at_end)
        FAIL();

    if (key)
        *key = hi->this->key;
    return hi->this->val;
}


/*
 * Expanding a hash table
 */

static void expand_array(c4_hash_t *ht)
{
    apr_pool_t *new_array_pool;
    apr_pool_t *old_array_pool;
    c4_hash_index_t *hi;
    c4_hash_entry_t **new_array;
    unsigned int new_max;

    if (apr_pool_create(&new_array_pool, ht->pool) != APR_SUCCESS)
        return; /* Give up and don't try to expand the array */
    old_array_pool = ht->array_pool;
    ht->array_pool = new_array_pool;

    new_max = ht->max * 2 + 1;
    new_array = alloc_array(ht, new_max);
    hi = c4_hash_iter_make(old_array_pool, ht);
    while (c4_hash_iter_next(hi)) {
        unsigned int i = hi->this->hash & new_max;
        hi->this->next = new_array[i];
        new_array[i] = hi->this;
    }
    ht->array = new_array;
    ht->max = new_max;

    apr_pool_destroy(old_array_pool);
}

/*
 * This is where we keep the details of the hash function and control
 * the maximum collision rate.
 *
 * If val is non-NULL it creates and initializes a new hash entry if
 * there isn't already one there; it returns an updatable pointer so
 * that hash entries can be removed.
 */

static c4_hash_entry_t **find_entry(c4_hash_t *ht,
                                    const void *key,
                                    void *val,
                                    bool *is_new)
{
    c4_hash_entry_t **hep, *he;
    unsigned int hash;

    if (is_new)
        *is_new = false;

    hash = ht->hash_func(key, ht->key_len, ht->user_data);

    /* scan linked list */
    for (hep = &ht->array[hash & ht->max], he = *hep;
         he; hep = &he->next, he = *hep) {
        if (he->hash == hash
            && ht->cmp_func(he->key, key, ht->key_len, ht->user_data))
            break;
    }
    if (he || !val)
        return hep;

    /* add a new entry for non-NULL values */
    if (is_new)
        *is_new = true;

    if ((he = ht->free) != NULL)
        ht->free = he->next;
    else
        he = apr_palloc(ht->pool, sizeof(*he));
    he->next = NULL;
    he->hash = hash;
    he->key  = key;
    he->val  = val;
    *hep = he;
    ht->count++;
    return hep;
}

void *c4_hash_get(c4_hash_t *ht, const void *key)
{
    c4_hash_entry_t *he;
    he = *find_entry(ht, key, NULL, NULL);
    if (he)
        return (void *) he->val;
    else
        return NULL;
}

void c4_hash_set(c4_hash_t *ht, const void *key, void *val)
{
    c4_hash_entry_t **hep;
    hep = find_entry(ht, key, val, NULL);
    if (*hep) {
        if (!val) {
            /* delete entry */
            c4_hash_entry_t *old = *hep;
            *hep = (*hep)->next;
            old->next = ht->free;
            ht->free = old;
            --ht->count;
        }
        else {
            /* replace entry */
            (*hep)->val = val;
            /* check that the collision rate isn't too high */
            if (ht->count > ht->max) {
                expand_array(ht);
            }
        }
    }
    /* else key not present and val==NULL */
}

bool c4_hash_remove(c4_hash_t *ht, const void *key)
{
    c4_hash_entry_t **hep;
    c4_hash_entry_t *old;

    hep = find_entry(ht, key, NULL, NULL);
    if (*hep == NULL)
        return false;

    /* Delete entry */
    old = *hep;
    *hep = (*hep)->next;
    old->next = ht->free;
    ht->free = old;
    --ht->count;
    return true;
}

void *c4_hash_set_if_new(c4_hash_t *ht, const void *key, void *val,
                         bool *is_new_p)
{
    c4_hash_entry_t *he;
    bool is_new;

    ASSERT(val != NULL);
    he = *find_entry(ht, key, val, &is_new);
    /* if new key, check that the collision rate isn't too high */
    if (is_new && ht->count > ht->max) {
        expand_array(ht);
    }
    if (is_new_p)
        *is_new_p = is_new;
    return (void *) he->val;
}

unsigned int c4_hash_count(c4_hash_t *ht)
{
    return ht->count;
}

void c4_hash_clear(c4_hash_t *ht)
{
    c4_hash_index_t  hix;
    c4_hash_index_t *hi;

    hi = &hix;
    hi->ht = ht;
    c4_hash_iter_reset(hi);

    while (c4_hash_iter_next(&hix))
        c4_hash_remove(ht, hi->this->key);
}
