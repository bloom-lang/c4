#include "col-internal.h"
#include "util/list.h"

/*
 * Return a freshly-allocated list. Since empty non-NULL lists are invalid,
 * we also allocate a single cell for the list; it is the caller's
 * responsibility to fill this in.
 */
static List *
new_list()
{
    List *result;
    ListCell *new_cell;

    new_cell = ol_alloc(sizeof(*new_cell));
    new_cell->next = NULL;
    /* new_cell->data is left undefined! */

    result = ol_alloc(sizeof(*result));
    result->length = 1;
    result->head = new_cell;
    result->tail = new_cell;

    return result;
}

static void
new_head_cell(List *list)
{
    ListCell *new_head;

    new_head = ol_alloc(sizeof(*new_head));
    new_head->next = list->head;

    list->head = new_head;
    list->length++;
}

static void
new_tail_cell(List *list)
{
    ListCell *new_tail;

    new_tail = ol_alloc(sizeof(*new_tail));
    new_tail->next = NULL;

    list->tail->next = new_tail;
    list->tail = new_tail;
    list->length++;
}

List *
list_append(List *list, void *datum)
{
    if (list == NULL)
        list = new_list();
    else
        new_tail_cell(list);

    lc_data(list->tail) = datum;
    return list;
}

List *
list_prepend(List *list, void *datum)
{
    if (list == NULL)
        list = new_list();
    else
        new_head_cell(list);

    lc_data(list->head) = datum;
    return list;
}
