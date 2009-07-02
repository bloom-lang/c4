#ifndef ROUTER_H
#define ROUTER_H

#include "planner/planner.h"
#include "types/tuple.h"

typedef struct ColRouter ColRouter;

ColRouter *router_make(ColInstance *col);
void router_destroy(ColRouter *router);

void router_start(ColRouter *router);

void router_enqueue_plan(ColRouter *router, ProgramPlan *plan);
void router_enqueue_tuple(ColRouter *router, Tuple *tuple,
                          const char *tbl_name);

#endif  /* ROUTER_H */
