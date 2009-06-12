/*
 * A simple linked list implementation. The code and interface is inspired
 * by the PostgreSQL linked list implementation. ~nrc
 */
#ifndef LIST_H
#define LIST_H

typedef struct ListCell ListCell;

typedef struct
{
    int length;
    ListCell *head;
    ListCell *tail;
} List;

struct ListCell
{
    void *data;
    ListCell *next;
};

/* Note that we doubly-evaluate the argument to these macros */
#define list_head(l)    ((l) ? l->head : NULL)
#define list_tail(l)    ((l) ? l->tail : NULL)
#define list_length(l)  ((l) ? l->length : 0)

#define list_make1(x1)				list_append(NULL, x1)
#define list_make2(x1,x2)			list_append(list_make1(x1), x2)
#define list_make3(x1,x2,x3)		list_append(list_make2(x1, x2), x3)
#define list_make4(x1,x2,x3,x4)		list_append(list_make3(x1, x2, x3), x4)

#define lc_data(lc)     ((lc)->data)
#define lc_next(lc)     ((lc)->next)

/*
 * foreach -
 *    a convenience macro which loops through the list
 */
#define foreach(cell, l)    \
    for ((cell) = list_head(l); (cell) != NULL; (cell) = lc_next(cell))

List *list_append(List *list, void *datum);
List *list_prepend(List *list, void *datum);

#endif  /* LIST_H */
