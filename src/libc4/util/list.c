#include "c4-internal.h"
#include "nodes/copyfuncs.h"
#include "util/list.h"

static ListCell *make_new_cell(List *list, ListCell *prev, ListCell *next);

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
    list->tail = make_new_cell(list, list->tail, NULL);
    lc_ptr(list->tail) = datum;
    return list;
}

List *
list_append_int(List *list, int datum)
{
    list->tail = make_new_cell(list, list->tail, NULL);
    lc_int(list->tail) = datum;
    return list;
}

List *
list_prepend(List *list, void *datum)
{
    list->head = make_new_cell(list, NULL, list->head);
    lc_ptr(list->head) = datum;
    return list;
}

List *
list_prepend_int(List *list, int datum)
{
    list->head = make_new_cell(list, NULL, list->head);
    lc_int(list->head) = datum;
    return list;
}

/*
 * Add a new cell to the list, with the given "prev" and "next" cells (which
 * might be NULL). The caller should fill-in the "data" field of the
 * returned ListCell.
 */
static ListCell *
make_new_cell(List *list, ListCell *prev, ListCell *next)
{
    ListCell *lc;

    ASSERT(list != NULL);

    lc = apr_palloc(list->pool, sizeof(*lc));
    lc->next = next;
    if (prev)
        prev->next = lc;

    list->length++;
    if (list->length == 1)
        list->head = list->tail = lc;

    return lc;
}

bool
list_member(List *list, const void *datum)
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

bool
list_member_str(List *list, const char *str)
{
    ListCell *lc;

    foreach (lc, list)
    {
        char *s = (char *) lc_ptr(lc);

        if (strcmp(s, str) == 0)
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

/*
 * Remove the specified cell from the list. "prev" should be the previous
 * cell in the list, or NULL to remove the head of the list. Note that the
 * removed cell is not free'd.
 */
void
list_remove_cell(List *list, ListCell *cell, ListCell *prev)
{
    if (prev)
    {
        ASSERT(prev->next == cell);
        prev->next = cell->next;
    }

    if (list->tail == cell)
        list->tail = prev;
    if (list->head == cell)
    {
        ASSERT(prev == NULL);
        list->head = cell->next;
    }

    list->length--;
}

/*
 * Return a shallow copy of "list", allocated from "pool". The data in the
 * input list cells is not copied, just the list structure.
 */
List *
list_copy(List *list, apr_pool_t *pool)
{
    List *result;
    ListCell *lc;

    result = list_make(pool);
    foreach (lc, list)
    {
        ListCell *new_tail = make_new_cell(result, list_tail(result), NULL);
        result->tail = new_tail;
        new_tail->data = lc->data;
    }

    return result;
}

/*
 * Return a deep copy of "list", allocated from "pool". The input list is
 * assumed to hold pointers to nodes that can be copied using copy_node().
 */
List *
list_copy_deep(List *list, apr_pool_t *pool)
{
    List *result;
    ListCell *lc;

    result = list_make(pool);
    foreach (lc, list)
    {
        void *data = lc_ptr(lc);
        list_append(result, copy_node(data, pool));
    }

    return result;
}

/*
 * Return a deep copy of "list", allocated from "pool". The input list is
 * assumed to hold pointers to NUL-terminated strings.
 */
List *
list_copy_str(List *list, apr_pool_t *pool)
{
    List *result;
    ListCell *lc;

    result = list_make(pool);
    foreach (lc, list)
    {
        char *str = (char *) lc_ptr(lc);
        list_append(result, apr_pstrdup(pool, str));
    }

    return result;
}

/*
 * Return "list" in reverse order. Note that we construct a new list
 * (allocating it from "pool"), but we don't deep-copy the contents of the
 * input list.
 */
List *
list_reverse(List *list, apr_pool_t *pool)
{
    List *result;
    ListCell *lc;

    result = list_make(pool);
    foreach (lc, list)
    {
        ListCell *new_head = make_new_cell(result, NULL, list_head(result));
        new_head->data = lc->data;
        result->head = new_head;
    }

    return result;
}
