#include "col-internal.h"
#include "parser/copyfuncs.h"
#include "util/list.h"

static ListCell *make_ptr_cell(List *list, void *datum,
                               ListCell *prev, ListCell *next);
static ListCell *make_int_cell(List *list, int datum,
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
    list->tail = make_ptr_cell(list, datum, list->tail, NULL);
    return list;
}

List *
list_append_int(List *list, int datum)
{
    list->tail = make_int_cell(list, datum, list->tail, NULL);
    return list;
}

List *
list_prepend(List *list, void *datum)
{
    list->head = make_ptr_cell(list, datum, NULL, list->head);
    return list;
}

List *
list_prepend_int(List *list, int datum)
{
    list->head = make_int_cell(list, datum, NULL, list->head);
    return list;
}

static ListCell *
make_ptr_cell(List *list, void *datum, ListCell *prev, ListCell *next)
{
    ListCell *lc;

    ASSERT(list != NULL);

    lc = apr_palloc(list->pool, sizeof(*lc));
    lc_ptr(lc) = datum;

    lc->next = next;
    if (prev)
        prev->next = lc;

    list->length++;
    if (list->length == 1)
        list->head = list->tail = lc;

    return lc;
}

static ListCell *
make_int_cell(List *list, int datum, ListCell *prev, ListCell *next)
{
    ListCell *lc;

    ASSERT(list != NULL);

    lc = apr_palloc(list->pool, sizeof(*lc));
    lc_int(lc) = datum;
    lc->next = next;

    if (prev)
        prev->next = lc;

    list->length++;
    if (list->length == 1)
        list->head = list->tail = lc;

    return lc;
}

bool
list_member(List *list, void *datum)
{
    ListCell *lc;

    foreach (lc, list)
    {
        if (lc_ptr(lc) == datum)
            return true;
    }

    return false;
}

bool
list_member_int(List *list, int datum)
{
    ListCell *lc;

    foreach (lc, list)
    {
        if (lc_int(lc) == datum)
            return true;
    }

    return false;
}

static ListCell *
list_get_cell(List *list, int idx)
{
    ListCell *lc;

    if (idx < 0 || idx >= list_length(list))
        FAIL();

    foreach (lc, list)
    {
        if (idx == 0)
            return lc;

        idx--;
    }

    FAIL();
    return NULL;        /* keep compiler quiet */
}

void *
list_get(List *list, int idx)
{
    return lc_ptr(list_get_cell(list, idx));
}

int
list_get_int(List *list, int idx)
{
    return lc_int(list_get_cell(list, idx));
}

static ListCell *
list_remove_first_cell(List *list)
{
    ListCell *head;

    if (list->length == 0)
        FAIL();

    head = list->head;
    list->head = head->next;
    list->length--;

    if (list->length == 0)
        list->tail = NULL;

    return head;
}

/*
 * XXX: Note that we leak the ListCell that is removed
 */
void *
list_remove_head(List *list)
{
    return lc_ptr(list_remove_first_cell(list));
}

int
list_remove_head_int(List *list)
{
    return lc_int(list_remove_first_cell(list));
}

List *
list_deep_copy(List *list, apr_pool_t *pool)
{
    List *result;
    ListCell *lc;

    result = list_make(pool);
    foreach (lc, list)
    {
        list_append(result, node_deep_copy(lc_ptr(lc), pool));
    }

    return result;
}
