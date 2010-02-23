/*
 * RSet is a refcounted set type: each element of the set has a count of the
 * number of times it has been inserted. The count is decremented on removal;
 * when the count reaches zero, the element is deleted from the set.
 *
 * RSet is based on C4Hash, as of rev fad4ec5d1b9e (r635).
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

#ifndef RSET_H
#define RSET_H

/**
 * Abstract type for refcounted sets.
 */
typedef struct rset_t rset_t;

/**
 * Abstract type for scanning refcounted sets.
 */
typedef struct rset_index_t rset_index_t;

/**
 * Callback functions for calculating hash values.
 * @param key The key.
 * @param klen The length of the key.
 * @param user_data User-specified opaque callback data
 */
typedef unsigned int (*rset_hashfunc_t)(const char *key, int klen,
                                        void *user_data);

/**
 * Callback functions for determining whether two elements are equal.
 * @param e1 First element
 * @param e2 Second element
 * @param elen The length of both elements.
 * @param user_data User-specified opaque callback data
 * @return Zero if the two elements should be considered equal, non-zero otherwise.
 */
typedef bool (*rset_keycomp_func_t)(const void *k1, const void *k2,
                                    int klen, void *user_data);

/**
 * The default hash function.
 */
unsigned int rset_hashfunc_default(const char *key, int klen, void *user_data);

/**
 * Create a refcounted set
 * @param pool The pool to allocate the rset out of
 * @param elem_len The length of elements in the table (must be fixed)
 * @param hash_func A custom hash function
 * @param cmp_func A custom key comparison function
 * @param cb_data Opaque user data that is passed to hash and cmp functions
 * @return The rset just created, or NULL if memory allocation failed
 */
rset_t *rset_make(apr_pool_t *pool, int elem_len, void *cb_data,
                  rset_hashfunc_t hash_func,
                  rset_keycomp_func_t cmp_func);

/**
 * Add an element to the rset. If the element is already present, bump its
 * refcount by 1 and return false; otherwise, add a new element with refcount 1,
 * and return true.
 * @param rs The rset
 * @param elem New element
 */
bool rset_add(rset_t *rs, void *elem);

/**
 * Look up the refcount of an element in the rset.
 * @param rs The rset
 * @param elem Element to search for
 * @return The refcount of the element, or 0 if the element is not found.
 */
unsigned int rset_get(rset_t *rs, void *elem);

/**
 * Decrement the refcount of an element in the rset. If the refcount reaches
 * zero, the element is removed.
 * @param rs The rset
 * @param elem Element to remove
 * @param new_rc The updated refcount of the element (might be zero). If the
 *               element wasn't found in the rset, this is not written-through.
 * @return NULL if the element was not found. 
 */
void *rset_remove(rset_t *rs, void *elem, unsigned int *new_refcount);

/**
 * Prepare to start iterating over the elements of an rset. This creates an
 * iterator and places it "before" the first element in the rset; that is,
 * rset_iter_make() + rset_next() is equivalent to a single call to
 * rset_first().
 * @param p The pool to allocate the rset_index_t iterator in. Must
 *          not be NULL.
 * @param rs The rset.
 */
rset_index_t *rset_iter_make(apr_pool_t *p, rset_t *rs);

/**
 * Reset the state of an rset iterator, so that it is placed "before" the first
 * element of the rset.
 */
void rset_iter_reset(rset_index_t *ri);

/**
 * Continue iterating over the entries in an rset.
 * @param hi The iteration state. This is written-through (modified).
 * @return true if there are any entries remaining in the rset, false otherwise.
 */
bool rset_iter_next(rset_index_t *ri);

/**
 * APR-style API to start iterating over the entries of an rset.
 * @param p The pool to allocate the rset_index_t iterator in. If NULL,
 *          an internal non-threadsafe iterator is used.
 * @param rs The rset.
 * @return Iteration state, or NULL if the rset is empty
 */
rset_index_t *rset_first(apr_pool_t *p, rset_t *rs);

/**
 * APR-style API for rset iteration.
 * @param ri The iteration state
 * @return The next updated iteration state, or NULL if out of elements.
 */
rset_index_t *rset_next(rset_index_t *ri);

/**
 * Get the current entry from the iteration state.
 * @param hi The iteration state
 * @return The current element
 */
void *rset_this(rset_index_t *ri);

/**
 * Get the number of elements in the rset.
 * @param rs The rset.
 * @return The number of elements in the rset.
 */
unsigned int rset_count(rset_t *rs);

/** @} */

#endif  /* !RSET_H */
