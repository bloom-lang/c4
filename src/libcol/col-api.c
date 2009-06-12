#include "col-internal.h"
#include "net/network.h"
#include "parser/parser.h"

ColInstance *
col_init()
{
    ColInstance *result = ol_alloc(sizeof(*result));
    result->net = network_init();
    return result;
}

ColStatus
col_destroy(ColInstance *col)
{
    network_destroy(col->net);
    ol_free(col);
    return COL_OK;
}

ColStatus
col_install_file(ColInstance *col, const char *path)
{
    return COL_OK;
}

ColStatus
col_install_str(ColInstance *col, const char *str)
{
    ColParser *parser;
    AstProgram *program;

    parser = parser_init(col);
    program = parser_do_parse(parser, str);
    parser_destroy(parser);

    return COL_OK;
}

void
col_start(ColInstance *col)
{
    network_start(col->net);
}
