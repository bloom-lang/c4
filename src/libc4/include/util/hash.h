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
 * The default hash function.
 */
unsigned int c4_hashfunc_default(const char *key, int klen, void *user_data);

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
void c4_hash_set(c4_hash_t *ht, const void *key, const void *val);

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
                         const void *val, bool *is_new);


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
 * hash table; that is, c4_hash_iter_make() + c4_hash_next() is
 * equivalent to a single call to c4_hash_first().
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
 * APR-style API to start iterating over the entries of a hash table.
 * @param p The pool to allocate the c4_hash_index_t iterator in. If NULL,
 *          an internal non-threadsafe iterator is used.
 * @param ht The hash table
 * @return Iteration state, or NULL if the hash table is empty
 */
c4_hash_index_t *c4_hash_first(apr_pool_t *p, c4_hash_t *ht);

/**
 * APR-style API for hash iteration.
 * @param hi The iteration state
 * @return The next updated iteration state, or NULL if out of elements.
 */
c4_hash_index_t *c4_hash_next(c4_hash_index_t *hi);

/**
 * Get the current entry's details from the iteration state.
 * @param hi The iteration state
 * @param key Return pointer for the pointer to the key.
 * @param val Return pointer for the associated value.
 * @remark The return pointers should point to a variable that will be set to the
 *         corresponding data, or they may be NULL if the data isn't interesting.
 */
void c4_hash_this(c4_hash_index_t *hi, const void **key, void **val);

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

/**
 * Declaration prototype for the iterator callback function of c4_hash_do().
 *
 * @param rec The data passed as the first argument to c4_hash_[v]do()
 * @param key The key from this iteration of the hash table
 * @param value The value from this iteration of the hash table
 * @remark Iteration continues while this callback function returns non-zero.
 * To export the callback function for c4_hash_do() it must be declared 
 * in the _NONSTD convention.
 */
typedef int (c4_hash_do_callback_fn_t)(void *rec, const void *key,
                                       const void *value);

/**
 * Iterate over a hash table running the provided function once for every
 * element in the hash table. The @param comp function will be invoked for
 * every element in the hash table.
 *
 * @param comp The function to run
 * @param rec The data to pass as the first argument to the function
 * @param ht The hash table to iterate over
 * @return FALSE if one of the comp() iterations returned zero; TRUE if all
 *            iterations returned non-zero
 * @see c4_hash_do_callback_fn_t
 */
int c4_hash_do(c4_hash_do_callback_fn_t *comp,
                void *rec, const c4_hash_t *ht);

/** @} */

#endif  /* !APR_HASH_H */
