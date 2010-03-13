/*
 * C4Hash is a hash table implementation based on the apr_hash code from
 * APR. This version taken from APR trunk, as of July 3 11:00 AM PST
 * (r791000). ~nrc
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

#ifndef C4_HASH_H
#define C4_HASH_H

/**
 * Abstract type for hash tables.
 */
typedef struct c4_hash_t c4_hash_t;

/**
 * Abstract type for scanning hash tables.
 */
typedef struct c4_hash_index_t c4_hash_index_t;

/**
 * Callback functions for calculating hash values.
 * @param key The key.
 * @param klen The length of the key.
 * @param user_data User-specified opaque callback data
 */
typedef unsigned int (*c4_hashfunc_t)(const char *key, int klen,
                                      void *user_data);

/**
 * Callback functions for determining whether two hash table keys are equal.
 * @param k1 First key
 * @param k2 Second key
 * @param klen The length of both keys.
 * @param user_data User-specified opaque callback data
 * @return Zero if the two keys should be considered equal, non-zero otherwise.
 */
typedef bool (*c4_keycomp_func_t)(const void *k1, const void *k2,
                                  int klen, void *user_data);

/**
 * Create a hash table
 * @param pool The pool to allocate the hash table out of
 * @param key_len The length of keys in the table (must be fixed)
 * @param hash_func A custom hash function
 * @param cmp_func A custom key comparison function
 * @param cb_data Opaque user data that is passed to hash and cmp functions
 * @return The hash table just created, or NULL if memory allocation failed
 */
c4_hash_t *c4_hash_make(apr_pool_t *pool, int key_len, void *cb_data,
                        c4_hashfunc_t hash_func,
                        c4_keycomp_func_t cmp_func);

/**
 * Associate a value with a key in a hash table.
 * @param ht The hash table
 * @param key Pointer to the key
 * @param val Value to associate with the key
 * @remark If the value is NULL the hash entry is deleted.
 */
void c4_hash_set(c4_hash_t *ht, const void *key, void *val);

/**
 * Associated a value with a key in the hash table, iff the key is not
 * already in the hash table.
 * @param ht The hash table
 * @param key Pointer to the key
 * @param val Value to associate with the key, if the key is not already in
 *            the hash table. Cannot be NULL.
 * @param is_new Output parameter; set to true iff key was not previously
 *               in hash table
 * @return The value associated with the key. If key is new, this is "val";
 *         otherwise, the hash table is unmodified, and the result is
 *         equivalent to c4_hash_get(ht, key).
 */
void *c4_hash_set_if_new(c4_hash_t *ht, const void *key,
                         void *val, bool *is_new);


/**
 * Look up the value associated with a key in a hash table.
 * @param ht The hash table
 * @param key Pointer to the key
 * @return Returns NULL if the key is not present.
 */
void *c4_hash_get(c4_hash_t *ht, const void *key);

/**
 * Remove an entry from the hash table.
 * @param ht The hash table
 * @param key Pointer to the key
 * @return true if the key was found (and deleted), false if the key was not
 *         found
 */
bool c4_hash_remove(c4_hash_t *ht, const void *key);

/**
 * Prepare to start iterating over the entries in a hash table. This
 * creates an iterator and places it "before" the first element in the
 * hash table.
 * @param p The pool to allocate the c4_hash_index_t iterator in. Must
 *          not be NULL.
 * @param ht The hash table
 */
c4_hash_index_t *c4_hash_iter_make(apr_pool_t *p, c4_hash_t *ht);

/**
 * Reset the state of a hash table iterator, so that it is placed
 * "before" the first element in the hash table.
 */
void c4_hash_iter_reset(c4_hash_index_t *hi);

/**
 * Continue iterating over the entries in a hash table.
 * @param hi The iteration state. This is written-through (modified).
 * @return true if there are any entries remaining in the hash table, false otherwise.
 */
bool c4_hash_iter_next(c4_hash_index_t *hi);

/**
 * Get the current key from the iteration state.
 * @param hi The iteration state
 * @return Pointer for the current key.
 */
const void *c4_hash_this_key(c4_hash_index_t *hi);

/**
 * Get the current value from the iteration state.
 * @param hi The iteration state
 * @return Pointer for the current value.
 */
void *c4_hash_this_val(c4_hash_index_t *hi);

/**
 * Get the number of key/value pairs in the hash table.
 * @param ht The hash table
 * @return The number of key/value pairs in the hash table.
 */
unsigned int c4_hash_count(c4_hash_t *ht);

/**
 * Clear any key/value pairs in the hash table.
 * @param ht The hash table
 */
void c4_hash_clear(c4_hash_t *ht);

/** @} */

#endif  /* !C4_HASH_H */
