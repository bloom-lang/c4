#ifndef ROUTER_H
#define ROUTER_H

#include "types/tuple.h"

typedef struct ColRouter ColRouter;

ColRouter *router_make(ColInstance *col);
void router_destroy(ColRouter *router);

void router_start(ColRouter *router);

void router_enqueue(ColRouter *router, Tuple *tuple);

#endif  /* ROUTER_H */
