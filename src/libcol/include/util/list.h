/*
 * A simple linked list implementation. The code and interface is inspired
 * by the PostgreSQL linked list implementation. ~nrc
 */
#ifndef LIST_H
#define LIST_H

typedef struct ListCell ListCell;

typedef struct
{
    apr_pool_t *pool;
    int length;
    ListCell *head;
    ListCell *tail;
} List;

#define list_head(list)         (list)->head
#define list_tail(list)         (list)->tail
#define list_length(list)       (list)->length
#define list_is_empty(list)     (list_length(list) == 0)

struct ListCell
{
    union
    {
        void *ptr_value;
        int int_value;
    } data;
    ListCell *next;
};

#define lc_ptr(lc)      ((lc)->data.ptr_value)
#define lc_int(lc)      ((lc)->data.int_value)

/*
 * foreach -
 *    a convenience macro which loops through the list
 */
#define foreach(cell, l)    \
    for ((cell) = l->head; (cell) != NULL; (cell) = (cell)->next)

List *list_make(apr_pool_t *pool);
#define list_make1(datum, pool)         list_append(list_make(pool), (datum))
#define list_make1_int(datum, pool)     list_append_int(list_make(pool), (datum))

List *list_append(List *list, void *datum);
List *list_append_int(List *list, int datum);

List *list_prepend(List *list, void *datum);
List *list_prepend_int(List *list, int datum);

bool list_member(List *list, void *datum);
bool list_member_int(List *list, int datum);

void *list_get(List *list, int idx);
int list_get_int(List *list, int idx);

void *list_remove_head(List *list);
int list_remove_head_int(List *list);

void list_remove_cell(List *list, ListCell *cell, ListCell *prev);

List *list_copy(List *list, apr_pool_t *pool);
List *list_copy_deep(List *list, apr_pool_t *pool);
List *list_copy_str(List *list, apr_pool_t *pool);

List *list_reverse(List *list, apr_pool_t *pool);

#endif  /* LIST_H */
