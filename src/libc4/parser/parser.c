#include "c4-internal.h"

#include "nodes/copyfuncs.h"
#include "parser/analyze.h"
/*
 * XXX: we need to include the definition of C4Parser before we can include
 * ol_scan.h, because Flex is broken.
 */
#include "parser/parser-internal.h"
#include "ol_parse.h"
#include "ol_scan.h"

static C4Parser *
parser_make(apr_pool_t *pool)
{
    apr_pool_t *parse_pool;
    C4Parser *parser;

    parse_pool = make_subpool(pool);
    parser = apr_pcalloc(parse_pool, sizeof(*parser));
    parser->pool = parse_pool;
    parser->lit_buf = sbuf_make(parser->pool);
    return parser;
}

static AstProgram *
do_parse(C4Parser *parser, const char *str)
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
    yy_delete_buffer(buf_state, parser->yyscanner);
    yylex_destroy(parser->yyscanner);

    if (parse_result)
        ERROR("Parsing failed");

    return parser->result;
}

static void
parser_destroy(C4Parser *parser)
{
    apr_pool_destroy(parser->pool);
}

AstProgram *
parse_str(const char *str, apr_pool_t *pool, C4Runtime *c4)
{
    C4Parser *parser;
    AstProgram *ast;

    parser = parser_make(pool);
    ast = do_parse(parser, str);
    analyze_ast(ast, parser->pool, c4);
    /* Copy the finished AST to the caller's pool */
    ast = copy_node(ast, pool);
    parser_destroy(parser);

    return ast;
}
