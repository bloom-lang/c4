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

void list_append(List *list, void *datum);
void list_prepend(List *list, void *datum);

#endif  /* LIST_H */
