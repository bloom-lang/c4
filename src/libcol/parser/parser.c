#include "col-internal.h"

#include "parser/analyze.h"
/*
 * XXX: we need to include the definition of ColParser before we can include
 * ol_scan.h, because Flex is broken.
 */
#include "parser/parser-internal.h"
#include "ol_scan.h"

static ColParser *
parser_make(ColInstance *col)
{
    apr_pool_t *pool;
    ColParser *parser;

    pool = make_subpool(col->pool);
    parser = apr_pcalloc(pool, sizeof(*parser));
    parser->pool = pool;
    return parser;
}

static AstProgram *
do_parse(ColParser *parser, const char *str)
{
    apr_size_t slen;
    char *scan_buf;
    YY_BUFFER_STATE buf_state;
    int parse_result;

    yylex_init_extra(parser, &parser->yyscanner);

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

static ColStatus
parser_destroy(ColParser *parser)
{
    apr_pool_destroy(parser->pool);
    return COL_OK;
}

AstProgram *
parse_str(ColInstance *col, const char *str, apr_pool_t *pool)
{
    ColParser *parser;
    AstProgram *ast;

    parser = parser_make(col);
    ast = do_parse(parser, str);
    analyze_ast(ast, parser->pool);
    /* Copy "ast" => "pool" */
    parser_destroy(parser);

    return NULL;
}
