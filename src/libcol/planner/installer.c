#include "col-internal.h"
#include "operator/filter.h"
#include "operator/insert.h"
#include "operator/scan.h"
#include "parser/copyfuncs.h"
#include "planner/installer.h"
#include "router.h"
#include "types/catalog.h"

typedef struct InstallState
{
    ColInstance *col;
    apr_pool_t *tmp_pool;
} InstallState;

static void
plan_install_defines(ProgramPlan *plan, InstallState *istate)
{
    ListCell *lc;

    foreach (lc, plan->defines)
    {
        AstDefine *def = (AstDefine *) lc_ptr(lc);

        cat_define_table(istate->col->cat, def->name,
                         def->schema, def->keys);
    }
}

static void
install_op_chain(OpChainPlan *chain_plan, InstallState *istate)
{
    List *chain_rev;
    Operator *prev_op;
    ListCell *lc;
    apr_pool_t *chain_pool;
    OpChain *op_chain;

    chain_pool = make_subpool(istate->col->pool);

    /*
     * We build the operator chain in reverse, so that each operator knows
     * which operator follows it in the op chain when it is created.
     */
    chain_rev = list_reverse(chain_plan->chain, istate->tmp_pool);
    prev_op = NULL;

    foreach (lc, chain_rev)
    {
        OpPlan *plan = (OpPlan *) lc_ptr(lc);
        Operator *op;

        switch (plan->op_kind)
        {
            case OPER_INSERT:
                /* Should be the last op in the chain */
                ASSERT(prev_op == NULL);
                op = (Operator *) insert_op_make((InsertOpPlan *) plan,
                                                 chain_pool);
                break;

            case OPER_FILTER:
                op = (Operator *) filter_op_make((FilterOpPlan *) plan,
                                                 prev_op, chain_pool);
                break;

            case OPER_SCAN:
                op = (Operator *) scan_op_make((ScanOpPlan *) plan,
                                               prev_op, chain_pool);
                break;

            default:
                ERROR("Unrecognized operator kind in plan: %d",
                      (int) plan->op_kind);
                op = NULL;      /* Keep compiler quiet */
        }

        prev_op = op;
    }

    op_chain = apr_pcalloc(chain_pool, sizeof(*op_chain));
    op_chain->pool = chain_pool;
    op_chain->delta_tbl = apr_pstrdup(chain_pool, chain_plan->delta_tbl);
    op_chain->head = copy_node(chain_plan->head, chain_pool);
    op_chain->length = list_length(chain_rev);
    op_chain->chain_start = prev_op;
    op_chain->next = NULL;

    router_add_op_chain(istate->col->router, op_chain);
}

static void
plan_install_rules(ProgramPlan *plan, InstallState *istate)
{
    ListCell *lc;

    foreach (lc, plan->rules)
    {
        RulePlan *rplan = (RulePlan *) lc_ptr(lc);
        ListCell *lc2;

        foreach (lc2, rplan->chains)
        {
            OpChainPlan *chain_plan = (OpChainPlan *) lc_ptr(lc);

            install_op_chain(chain_plan, istate);
        }
    }
}

static Tuple *
fact_make_tuple(AstFact *fact, InstallState *istate)
{
    return NULL;
}

static void
plan_install_facts(ProgramPlan *plan, InstallState *istate)
{
    ListCell *lc;

    foreach (lc, plan->facts)
    {
        AstFact *fact = (AstFact *) lc_ptr(lc);
        Tuple *t;

        t = fact_make_tuple(fact, istate);
        router_enqueue_tuple(istate->col->router, t, fact->head->name);
        tuple_unpin(t);
    }
}

static InstallState *
make_istate(ColInstance *col)
{
    apr_pool_t *tmp_pool;
    InstallState *istate;

    tmp_pool = make_subpool(col->pool);
    istate = apr_pcalloc(tmp_pool, sizeof(*istate));
    istate->tmp_pool = tmp_pool;
    istate->col = col;

    return istate;
}

static void
destroy_istate(InstallState *istate)
{
    apr_pool_destroy(istate->tmp_pool);
}

void
install_plan(ProgramPlan *plan, ColInstance *col)
{
    InstallState *istate;

    istate = make_istate(col);
    plan_install_defines(plan, istate);
    plan_install_rules(plan, istate);
    plan_install_facts(plan, istate);
    destroy_istate(istate);
}
