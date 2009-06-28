#ifndef OPERATOR_H
#define OPERATOR_H

typedef struct Operator
{
    struct Operator *next;
    apr_pool_t *pool;
} Operator;

#endif  /* OPERATOR_H */
