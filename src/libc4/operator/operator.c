#include "c4-internal.h"
#include "nodes/copyfuncs.h"
#include "operator/operator.h"

Operator *
operator_make(C4NodeKind kind, apr_size_t sz, PlanNode *plan,
              Operator *next_op, OpChain *chain,
              op_invoke_func invoke_f)
{
    apr_pool_t *pool = chain->pool;
    Operator *op;
    ListCell *lc;
    int i;

    op = apr_pcalloc(pool, sz);
    op->node.kind = kind;
    op->pool = pool;
    op->plan = copy_node(plan, pool);
    op->next = next_op;
    op->chain = chain;
    op->exec_cxt = apr_pcalloc(pool, sizeof(*op->exec_cxt));
    op->invoke = invoke_f;

    op->nproj = list_length(op->plan->proj_list);
    op->proj_ary = apr_palloc(pool, sizeof(ExprState *) * op->nproj);

    i = 0;
    foreach (lc, op->plan->proj_list)
    {
        ExprNode *expr = (ExprNode *) lc_ptr(lc);

        op->proj_ary[i++] = make_expr_state(expr, op->exec_cxt, pool);
    }

    op->proj_schema = schema_make_from_exprs(op->nproj, op->proj_ary,
                                             chain->c4, pool);

    return op;
}

Tuple *
operator_do_project(Operator *op)
{
    Tuple *proj_tuple;
    int i;

    proj_tuple = tuple_make_empty(op->proj_schema);
    for (i = 0; i < op->nproj; i++)
    {
        ExprState *proj_state = op->proj_ary[i];
        Datum result;

        result = eval_expr(proj_state);
        proj_tuple->vals[i] = datum_copy(result, proj_state->expr->type);
    }

    return proj_tuple;
}

OpChainList *
opchain_list_make(apr_pool_t *pool)
{
    OpChainList *result;

    result = apr_palloc(pool, sizeof(*result));
    result->length = 0;
    result->head = NULL;

    return result;
}

void
opchain_list_add(OpChainList *list, OpChain *op_chain)
{
    ASSERT(op_chain->next == NULL);

    op_chain->next = list->head;
    list->head = op_chain;
    list->length++;
}
