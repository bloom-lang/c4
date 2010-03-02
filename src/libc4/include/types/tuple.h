#ifndef TUPLE_H
#define TUPLE_H

#include "types/catalog.h"
#include "types/datum.h"
#include "types/schema.h"
#include "util/strbuf.h"

/*
 * Note that tuple refcounts are not currently tracked by the APR pool cleanup
 * mechanism. Therefore, modules keeping a tuple pinned should be sure to
 * release their pin in a cleanup function.
 */
typedef struct Tuple
{
    /*
     * When tuples are on a TuplePool freelist, we need a way to record the next
     * element of the freelist. Since Tuples on the freelist have refcount zero
     * by definition, we just reuse the "refcount" struct element. Due to
     * padding, it probably doesn't cost any extra space.
     *
     * TODO: On LP64 machines, we have 6 bytes of padding we can use. One
     * possible use for them is to cache the per-Tuple hash code.
     */
    union
    {
        struct Tuple *next_free;
        apr_uint16_t refcount;
    } u;

    Datum vals[1];      /* Variable-length array */
} Tuple;

#define tuple_get_val(t, i)     ((t)->vals[(i)])

Tuple *tuple_make_empty(Schema *s);
Tuple *tuple_make(Schema *s, Datum *values);
Tuple *tuple_make_from_strings(Schema *s, char **values);

void tuple_pin(Tuple *tuple);
void tuple_unpin(Tuple *tuple, Schema *s);

bool tuple_equal(Tuple *t1, Tuple *t2, Schema *s);
apr_uint32_t tuple_hash(Tuple *t, Schema *s);
/* For use in hash tables */
bool tuple_cmp_tbl(const void *k1, const void *k2, int klen, void *data);
unsigned int tuple_hash_tbl(const char *key, int klen, void *data);

bool tuple_is_remote(Tuple *tuple, TableDef *tbl_def, C4Runtime *c4);

char *tuple_to_str(Tuple *tuple, Schema *s, apr_pool_t *pool);
void tuple_to_str_buf(Tuple *tuple, Schema *s, StrBuf *buf);
void tuple_to_buf(Tuple *tuple, Schema *s, StrBuf *buf);
Tuple *tuple_from_buf(StrBuf *buf, Schema *s);
char *tuple_to_sql_insert_str(Tuple *tuple, Schema *s, apr_pool_t *pool);

#endif  /* TUPLE_H */
