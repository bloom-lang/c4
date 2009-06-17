#include "col-internal.h"
#include "util/list.h"

static ListCell *make_new_cell(List *list, void *datum,
                               ListCell *prev, ListCell *next);

List *
list_make(apr_pool_t *pool)
{
    List *result;

    result = apr_palloc(pool, sizeof(*result));
    result->pool = pool;
    result->length = 0;
    result->head = NULL;
    result->tail = NULL;

    return result;
}

List *
list_append(List *list, void *datum)
{
    list->tail = make_new_cell(list, datum, list->tail, NULL);
    return list;
}

List *
list_prepend(List *list, void *datum)
{
    list->head = make_new_cell(list, datum, NULL, list->head);
    return list;
}

static ListCell *
make_new_cell(List *list, void *datum, ListCell *prev, ListCell *next)
{
    ListCell *lc;

    ASSERT(list != NULL);

    lc = apr_palloc(list->pool, sizeof(*lc));
    lc->data = datum;
    lc->next = next;

    if (prev)
        prev->next = datum;

    return lc;
}
