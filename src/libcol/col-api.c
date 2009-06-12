#include "col-internal.h"

#include "parser/parser.h"

ColInstance *
col_init()
{
    ColInstance *result = ol_alloc(sizeof(*result));
    return result;
}

ColStatus
col_destroy(ColInstance *col)
{
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
