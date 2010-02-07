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
    unsigned int      hash;
    const void       *key;
    const void       *val;
};

/*
 * Data structure for iterating through a hash table.
 *
 * We keep a pointer to the next hash entry here to allow the current
 * hash entry to be freed or otherwise mangled between calls to
 * c4_hash_next().
 */
struct c4_hash_index_t {
    c4_hash_t         *ht;
    c4_hash_entry_t   *this, *next;
    unsigned int        index;
    int                 at_end;
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
    c4_hash_index_t     iterator;  /* For c4_hash_first(NULL, ...) */
    unsigned int        count, max;
    apr_ssize_t          key_len;
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

c4_hash_t *c4_hash_make(apr_pool_t *pool, apr_ssize_t key_len,
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
    ht->hash_func = hash_func;
    ht->cmp_func = cmp_func;
    return ht;
}

/*
 * Hash iteration functions.
 */

c4_hash_index_t *c4_hash_next(c4_hash_index_t *hi)
{
    if (c4_hash_iter_next(hi))
        return hi;
    else
        return NULL;
}

c4_hash_index_t *c4_hash_first(apr_pool_t *p, c4_hash_t *ht)
{
    c4_hash_index_t *hi;
    if (p)
        hi = apr_palloc(p, sizeof(*hi));
    else
        hi = &ht->iterator;

    hi->ht = ht;
    c4_hash_iter_reset(hi);
    return c4_hash_next(hi);
}

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
    hi->at_end = 0;
}

int c4_hash_iter_next(c4_hash_index_t *hi)
{
    if (hi->at_end)
        return 0;

    hi->this = hi->next;
    while (!hi->this) {
        if (hi->index > hi->ht->max) {
            hi->at_end = 1;
            return 0;
        }

        hi->this = hi->ht->array[hi->index++];
    }
    hi->next = hi->this->next;
    return 1;
}

void c4_hash_this(c4_hash_index_t *hi,
                  const void **key,
                  void **val)
{
    if (hi->at_end)
        FAIL();

    if (key)  *key  = hi->this->key;
    if (val)  *val  = (void *)hi->this->val;
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
    for (hi = c4_hash_first(NULL, ht); hi; hi = c4_hash_next(hi)) {
        unsigned int i = hi->this->hash & new_max;
        hi->this->next = new_array[i];
        new_array[i] = hi->this;
    }
    ht->array = new_array;
    ht->max = new_max;

    apr_pool_destroy(old_array_pool);
}

unsigned int c4_hashfunc_default(const char *char_key, apr_ssize_t klen)
{
    unsigned int hash = 0;
    const unsigned char *key = (const unsigned char *) char_key;
    const unsigned char *p;
    apr_ssize_t i;
    
    /*
     * This is the popular `times 33' hash algorithm which is used by
     * perl and also appears in Berkeley DB. This is one of the best
     * known hash functions for strings because it is both computed
     * very fast and distributes very well.
     *
     * The originator may be Dan Bernstein but the code in Berkeley DB
     * cites Chris Torek as the source. The best citation I have found
     * is "Chris Torek, Hash function for text in C, Usenet message
     * <27038@mimsy.umd.edu> in comp.lang.c , October, 1990." in Rich
     * Salz's USENIX 1992 paper about INN which can be found at
     * <http://citeseer.nj.nec.com/salz92internetnews.html>.
     *
     * The magic of number 33, i.e. why it works better than many other
     * constants, prime or not, has never been adequately explained by
     * anyone. So I try an explanation: if one experimentally tests all
     * multipliers between 1 and 256 (as I did while writing a low-level
     * data structure library some time ago) one detects that even
     * numbers are not useable at all. The remaining 128 odd numbers
     * (except for the number 1) work more or less all equally well.
     * They all distribute in an acceptable way and this way fill a hash
     * table with an average percent of approx. 86%.
     *
     * If one compares the chi^2 values of the variants (see
     * Bob Jenkins ``Hashing Frequently Asked Questions'' at
     * http://burtleburtle.net/bob/hash/hashfaq.html for a description
     * of chi^2), the number 33 not even has the best value. But the
     * number 33 and a few other equally good numbers like 17, 31, 63,
     * 127 and 129 have nevertheless a great advantage to the remaining
     * numbers in the large set of possible multipliers: their multiply
     * operation can be replaced by a faster operation based on just one
     * shift plus either a single addition or subtraction operation. And
     * because a hash function has to both distribute good _and_ has to
     * be very fast to compute, those few numbers should be preferred.
     *
     *                  -- Ralf S. Engelschall <rse@engelschall.com>
     */
    for (p = key, i = klen; i; i--, p++) {
        hash = hash * 33 + *p;
    }

    return hash;
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
                                    const void *val)
{
    c4_hash_entry_t **hep, *he;
    unsigned int hash;

    hash = ht->hash_func(key, ht->key_len);

    /* scan linked list */
    for (hep = &ht->array[hash & ht->max], he = *hep;
         he; hep = &he->next, he = *hep) {
        if (he->hash == hash
            && ht->cmp_func(he->key, key, ht->key_len) == 0)
            break;
    }
    if (he || !val)
        return hep;

    /* add a new entry for non-NULL values */
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

c4_hash_t *c4_hash_copy(apr_pool_t *pool, const c4_hash_t *orig)
{
    apr_pool_t *array_pool;
    c4_hash_t *ht;
    c4_hash_entry_t *new_vals;
    unsigned int i, j;

    if (apr_pool_create(&array_pool, pool) != APR_SUCCESS)
        return NULL;

    ht = apr_palloc(pool, sizeof(c4_hash_t) +
                    sizeof(c4_hash_entry_t) * orig->count);
    ht->pool = pool;
    ht->array_pool = array_pool;
    ht->free = NULL;
    ht->count = orig->count;
    ht->max = orig->max;
    ht->key_len = orig->key_len;
    ht->hash_func = orig->hash_func;
    ht->cmp_func = orig->cmp_func;
    ht->array = alloc_array(ht, ht->max);

    new_vals = (c4_hash_entry_t *)((char *)(ht) + sizeof(c4_hash_t));
    j = 0;
    for (i = 0; i <= ht->max; i++) {
        c4_hash_entry_t **new_entry = &(ht->array[i]);
        c4_hash_entry_t *orig_entry = orig->array[i];
        while (orig_entry) {
            *new_entry = &new_vals[j++];
            (*new_entry)->hash = orig_entry->hash;
            (*new_entry)->key = orig_entry->key;
            (*new_entry)->val = orig_entry->val;
            new_entry = &((*new_entry)->next);
            orig_entry = orig_entry->next;
        }
        *new_entry = NULL;
    }
    return ht;
}

void *c4_hash_get(c4_hash_t *ht, const void *key)
{
    c4_hash_entry_t *he;
    he = *find_entry(ht, key, NULL);
    if (he)
        return (void *) he->val;
    else
        return NULL;
}

void c4_hash_set(c4_hash_t *ht, const void *key, const void *val)
{
    c4_hash_entry_t **hep;
    hep = find_entry(ht, key, val);
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

void *c4_hash_set_if_new(c4_hash_t *ht, const void *key, const void *val)
{
    c4_hash_entry_t *he;

    ASSERT(val != NULL);
    he = *find_entry(ht, key, val);
    /* if new key, check that the collision rate isn't too high */
    if (he->val == val && ht->count > ht->max) {
        expand_array(ht);
    }
    return (void *)he->val;
}

unsigned int c4_hash_count(c4_hash_t *ht)
{
    return ht->count;
}

void c4_hash_clear(c4_hash_t *ht)
{
    c4_hash_index_t *hi;
    for (hi = c4_hash_first(NULL, ht); hi; hi = c4_hash_next(hi))
        c4_hash_set(ht, hi->this->key, NULL);
}

/* This is basically the following...
 * for every element in hash table {
 *    comp elemeny.key, element.value
 * }
 *
 * Like with apr_table_do, the comp callback is called for each and every
 * element of the hash table.
 */
int c4_hash_do(c4_hash_do_callback_fn_t *comp, void *rec, const c4_hash_t *ht)
{
    c4_hash_index_t  hix;
    c4_hash_index_t *hi;
    int rv, dorv  = 1;

    hix.ht = (c4_hash_t *)ht;
    c4_hash_iter_reset(&hix);

    if ((hi = c4_hash_next(&hix))) {
        /* Scan the entire table */
        do {
            rv = (*comp)(rec, hi->this->key, hi->this->val);
        } while (rv && (hi = c4_hash_next(hi)));

        if (rv == 0) {
            dorv = 0;
        }
    }
    return dorv;
}
