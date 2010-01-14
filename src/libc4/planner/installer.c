#include "c4-internal.h"
#include "nodes/copyfuncs.h"
#include "operator/filter.h"
#include "operator/insert.h"
#include "operator/scan.h"
#include "planner/installer.h"
#include "router.h"
#include "types/catalog.h"

typedef struct InstallState
{
    C4Runtime *c4;
    apr_pool_t *tmp_pool;
} InstallState;

static void
plan_install_defines(ProgramPlan *plan, InstallState *istate)
{
    ListCell *lc;

    foreach (lc, plan->defines)
    {
        AstDefine *def = (AstDefine *) lc_ptr(lc);

        cat_define_table(istate->c4->cat, def->name, def->storage,
                         def->schema, def->keys);
    }
}

static void
print_op_chain(OpChainPlan *chain_plan)
{
    List *chain = chain_plan->chain;
    ListCell *lc;

    printf("Chain: delta_tbl = %s, [", chain_plan->delta_tbl->ref->name);

    foreach (lc, chain)
    {
        C4Node *node = (C4Node *) lc_ptr(lc);

        printf("%s", node_get_kind_str(node));
        if (lc != list_tail(chain))
            printf(" => ");
    }

    printf("]\n");
}

static void
install_op_chain(OpChainPlan *chain_plan, InstallState *istate)
{
    List *chain_rev;
    Operator *prev_op;
    ListCell *lc;
    apr_pool_t *chain_pool;
    OpChain *op_chain;

#if 0
    print_op_chain(chain_plan);
#endif

    chain_pool = make_subpool(istate->c4->pool);

    op_chain = apr_pcalloc(chain_pool, sizeof(*op_chain));
    op_chain->pool = chain_pool;
    op_chain->c4 = istate->c4;
    op_chain->delta_tbl = cat_get_table(op_chain->c4->cat,
                                        chain_plan->delta_tbl->ref->name);
    op_chain->head = copy_node(chain_plan->head, chain_pool);
    op_chain->length = list_length(chain_plan->chain);
    op_chain->next = NULL;

    /*
     * We build the operator chain in reverse, so that each operator knows
     * which operator follows it in the op chain when it is created.
     */
    chain_rev = list_reverse(chain_plan->chain, istate->tmp_pool);
    prev_op = NULL;

    foreach (lc, chain_rev)
    {
        PlanNode *plan = (PlanNode *) lc_ptr(lc);
        Operator *op;

        ASSERT(list_length(plan->quals) == list_length(plan->qual_exprs));
#if 0
        print_plan_info(plan, istate->tmp_pool);
#endif

        switch (plan->node.kind)
        {
            case PLAN_FILTER:
                op = (Operator *) filter_op_make((FilterPlan *) plan,
                                                 prev_op, op_chain);
                break;

            case PLAN_INSERT:
                /* Should be the last op in the chain */
                ASSERT(prev_op == NULL);
                op = (Operator *) insert_op_make((InsertPlan *) plan, op_chain);
                break;

            case PLAN_SCAN:
                op = (Operator *) scan_op_make((ScanPlan *) plan,
                                               prev_op, op_chain);
                break;

            default:
                ERROR("Unrecognized operator kind in plan: %d",
                      (int) plan->node.kind);
        }

        prev_op = op;
    }
    op_chain->chain_start = prev_op;

    router_add_op_chain(istate->c4->router, op_chain);
#if 0
    printf("================\n");
#endif
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
            OpChainPlan *chain_plan = (OpChainPlan *) lc_ptr(lc2);

            install_op_chain(chain_plan, istate);
        }
    }
}

static Tuple *
fact_make_tuple(AstFact *fact, TableDef *tbl_def, InstallState *istate)
{
    Schema *schema;
    char **values;
    int i;
    ListCell *lc;

    schema = tbl_def->schema;
    values = apr_palloc(istate->tmp_pool, schema->len * sizeof(*values));
    i = 0;
    foreach (lc, fact->head->cols)
    {
        AstColumnRef *cref = (AstColumnRef *) lc_ptr(lc);
        AstConstExpr *c_expr = (AstConstExpr *) cref->expr;

        values[i] = c_expr->value;
        i++;
    }

    return tuple_make_from_strings(schema, values);
}

static void
plan_install_facts(ProgramPlan *plan, InstallState *istate)
{
    ListCell *lc;

    foreach (lc, plan->facts)
    {
        AstFact *fact = (AstFact *) lc_ptr(lc);
        TableDef *tbl_def;
        Tuple *t;

        tbl_def = cat_get_table(istate->c4->cat, fact->head->name);
        t = fact_make_tuple(fact, tbl_def, istate);
        router_install_tuple(istate->c4->router, t, tbl_def, true);
        tuple_unpin(t);
    }
}

static InstallState *
istate_make(apr_pool_t *pool, C4Runtime *c4)
{
    InstallState *istate;

    istate = apr_pcalloc(pool, sizeof(*istate));
    istate->tmp_pool = pool;
    istate->c4 = c4;

    return istate;
}

void
install_plan(ProgramPlan *plan, apr_pool_t *pool, C4Runtime *c4)
{
    InstallState *istate;

    istate = istate_make(pool, c4);
    plan_install_defines(plan, istate);
    plan_install_rules(plan, istate);
    plan_install_facts(plan, istate);
}
