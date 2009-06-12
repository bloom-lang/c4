#include "col-internal.h"

#include "parser/parser.h"

struct ColParser
{
    int foo;
};

ColParser *
parser_init(ColInstance *col)
{
    ColParser *result = ol_alloc(sizeof(*result));
    return result;
}

AstProgram *
parser_do_parse(ColParser *parser, const char *src)
{
    return NULL;
}

ColStatus
parser_destroy(ColParser *parser)
{
    ol_free(parser);
    return COL_OK;
}
