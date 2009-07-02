#include "col-internal.h"
#include "router.h"
#include "planner/installer.h"

static void
plan_install_defines(ProgramPlan *plan, ColInstance *col)
{
    ;
}

static void
plan_install_rules(ProgramPlan *plan, ColInstance *col)
{
    ;
}

static Tuple *
fact_make_tuple(AstFact *fact, ColInstance *col)
{
    return NULL;
}

static void
plan_install_facts(ProgramPlan *plan, ColInstance *col)
{
    ListCell *lc;

    foreach (lc, plan->facts)
    {
        AstFact *fact = (AstFact *) lc_ptr(lc);
        Tuple *t;

        t = fact_make_tuple(fact, col);
        router_enqueue_tuple(col->router, t, fact->head->name);
        tuple_unpin(t);
    }
}

void
install_plan(ProgramPlan *plan, ColInstance *col)
{
    plan_install_defines(plan, col);
    plan_install_rules(plan, col);
    plan_install_facts(plan, col);
}
