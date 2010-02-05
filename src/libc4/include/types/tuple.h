#ifndef TUPLE_H
#define TUPLE_H

#include "types/catalog.h"
#include "types/datum.h"
#include "types/schema.h"
#include "util/strbuf.h"

/*
 * XXX: Note that tuple refcounts are not currently tracked by the APR pool
 * cleanup mechanism. Therefore, modules keeping a tuple pinned should be
 * sure to release their pin in a cleanup function.
 */
typedef struct Tuple
{
    /*
     * When tuples are on a TuplePool freelist, we need a way to record the
     * next element of the freelist. Since we don't care about the schema of
     * Tuples on the freelist, we just reuse this struct element. We could
     * also use the refcount, of course, but that field might not be large
     * enough to store a pointer.
     */
    union
    {
        /*
         * XXX: Our dataflow is strictly typed, so we don't actually need to
         * store the schema in the tuple (other than for convenience).  XXX:
         * Refcount this? Or deep-copy it?
         */
        Schema *schema;
        struct Tuple *next_free;
    } ptr;

    apr_uint16_t refcount;
    Datum vals[1];      /* Variable-length array */
} Tuple;

#define tuple_get_schema(t)     ((t)->ptr.schema)
#define tuple_get_val(t, i)     ((t)->vals[(i)])

Tuple *tuple_make_empty(Schema *s);
Tuple *tuple_make(Schema *s, Datum *values);
Tuple *tuple_make_from_strings(Schema *s, char **values);

void tuple_pin(Tuple *tuple);
void tuple_unpin(Tuple *tuple);

bool tuple_equal(Tuple *t1, Tuple *t2);
apr_uint32_t tuple_hash(Tuple *t);

bool tuple_is_remote(Tuple *tuple, TableDef *tbl_def, C4Runtime *c4);

char *tuple_to_str(Tuple *tuple, apr_pool_t *pool);
void tuple_to_str_buf(Tuple *tuple, StrBuf *buf);
void tuple_to_buf(Tuple *tuple, StrBuf *buf);
Tuple *tuple_from_buf(StrBuf *buf, TableDef *tbl_def);
char *tuple_to_sql_insert_str(Tuple *tuple, apr_pool_t *pool);

#endif  /* TUPLE_H */
