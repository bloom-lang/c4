#include "col-internal.h"

#include "ol_scan.h"
#include "parser/parser-internal.h"

ColParser *
parser_make(ColInstance *col)
{
    apr_pool_t *pool;
    ColParser *parser;

    pool = make_subpool(col->pool);
    parser = apr_pcalloc(pool, sizeof(*parser));
    parser->pool = pool;
    return parser;
}

AstProgram *
parser_do_parse(ColParser *parser, const char *str)
{
    apr_size_t slen;
    char *scan_buf;
    YY_BUFFER_STATE buf_state;
    int parse_result;

    yylex_init(&parser->yyscanner);

    slen = strlen(str);
    scan_buf = setup_scan_buf(str, slen, parser->pool);
    buf_state = yy_scan_buffer(scan_buf, slen + 2, parser->yyscanner);

    parse_result = yyparse(parser, parser->yyscanner);
    if (parse_result)
    {
        yy_delete_buffer(buf_state, parser->yyscanner);
        yylex_destroy(parser->yyscanner);
        return NULL;
    }

    yy_delete_buffer(buf_state, parser->yyscanner);
    yylex_destroy(parser->yyscanner);
    return parser->result;
}

ColStatus
parser_destroy(ColParser *parser)
{
    apr_pool_destroy(parser->pool);
    return COL_OK;
}
