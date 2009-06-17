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

struct ListCell
{
    void *data;
    ListCell *next;
};

/*
 * foreach -
 *    a convenience macro which loops through the list
 */
#define foreach(cell, l)    \
    for ((cell) = l->head; (cell) != NULL; (cell) = (cell)->next)

List *list_make(apr_pool_t *pool);
#define list_make1(datum, pool)         list_append(list_make(pool), datum)

List *list_append(List *list, void *datum);
List *list_prepend(List *list, void *datum);

#endif  /* LIST_H */
