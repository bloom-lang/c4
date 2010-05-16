%{
#include <string.h>

#include "c4-internal.h"
#include "parser/parser-internal.h"

#include "ol_parse.h"

#define SCANNER_POOL yyget_extra(yyscanner)->pool

static unsigned char unescape_char(unsigned char c);
%}

%option 8bit
%option never-interactive batch
%option fast
%option nounput
%option noyywrap
%option reentrant
%option bison-bridge
%option extra-type="C4Parser *"

digit           [0-9]
space           [ \t\n\r\f]
newline         [\n\r]
non_newline     [^\n\r]

/* "self" is the set of characters that are returned to the parser as-is. */
self            [,()\[\].;\:\+\-\*\/\%\^\=@\{\}\<\>]

comment         ("//"{non_newline}*)
whitespace      ({space}+|{comment})

/*
 * Note that we distinguish between "table identifiers" and "variable
 * identifiers"; the former must be entirely in lower-case, whereas the
 * latter must begin with an upperchase letter. This is partly to simplify
 * parsing, but arguably it enforces good naming discipline.
 *
 * XXX: No support for Unicode identifiers
 */
vident_start    [A-Z_]
vident_cont     [A-Za-z_0-9]
var_ident       {vident_start}{vident_cont}*

tident_start    [a-z]
tident_cont     [a-z_0-9]
tbl_ident       {tident_start}{tident_cont}*

integer         {digit}+
decimal         {digit}+\.{digit}*
real            ({integer}|{decimal})([Ee][-+]?{digit}+)?

%x comment
%x string

%%
"/*"                    { BEGIN(comment); }
<comment>[^*]*          /* eat anything that's not a '*' */
<comment>"*"+[^*/]*     /* eat up '*'s not followed by '/'s */
<comment>"*"+"/"        { BEGIN(INITIAL); }

\" {
    C4Parser *context = yyget_extra(yyscanner);
    sbuf_reset(context->lit_buf);
    BEGIN(string);
}

<string>\\. {
    C4Parser *context = yyget_extra(yyscanner);
    sbuf_append_char(context->lit_buf, unescape_char((unsigned char) yytext[1]));
}

<string>[^\\\"]+ {
    C4Parser *context = yyget_extra(yyscanner);
    sbuf_append_data(context->lit_buf, yytext, yyleng);
}

<string>\" {
    C4Parser *context = yyget_extra(yyscanner);
    yylval->str = sbuf_dup(context->lit_buf, SCANNER_POOL);
    BEGIN(INITIAL);
    return SCONST;
}

\'[^\\\']\' {
    yylval->str = apr_pstrmemdup(SCANNER_POOL, &yytext[1], 1);
    return CCONST;
}

\'\\.\' {
    char c = unescape_char((unsigned char) yytext[2]);
    yylval->str = apr_pstrmemdup(SCANNER_POOL, &c, 1);
    return CCONST;
}

{whitespace}            /* ignore */
<INITIAL><<EOF>>        { yyterminate(); }

{self}                  { return yytext[0]; }

"<="                    { return OL_LTE; }
">="                    { return OL_GTE; }
"!="                    { return OL_NEQ; }
"<>"                    { return OL_NEQ; }
"=="                    { return OL_EQ; }

"avg"                   { return OL_AVG; }
"count"                 { return OL_COUNT; }
"max"                   { return OL_MAX; }
"min"                   { return OL_MIN; }
"sum"                   { return OL_SUM; }

"define"                { return DEFINE; }
"delete"                { return DELETE; }
"false"                 { return OL_FALSE; }
"memory"                { return MEMORY; }
"notin"                 { return NOTIN; }
"sqlite"                { return SQLITE; }
"timer"                 { return TIMER; }
"true"                  { return OL_TRUE; }

{integer} {
    yylval->str = apr_pstrmemdup(SCANNER_POOL, yytext, yyleng);
    return ICONST;
}

{real} {
    yylval->str = apr_pstrmemdup(SCANNER_POOL, yytext, yyleng);
    return FCONST;
}

{var_ident} {
    yylval->str = apr_pstrmemdup(SCANNER_POOL, yytext, yyleng);
    return VAR_IDENT;
}

{tbl_ident} {
    yylval->str = apr_pstrmemdup(SCANNER_POOL, yytext, yyleng);
    return TBL_IDENT;
}

%%

/*
 * Setup an input buffer for Flex to scan. This is defined here mostly so
 * that we can use the YY_END_OF_BUFFER_CHAR #define, which Flex annoyingly
 * doesn't export. Return value is apr_palloc'd in the given pool.
 */
char *
setup_scan_buf(const char *str, apr_size_t len, apr_pool_t *pool)
{
    char *scan_buf;

    scan_buf = apr_palloc(pool, len + 2);
    memcpy(scan_buf, str, len);
    scan_buf[len] = scan_buf[len + 1] = YY_END_OF_BUFFER_CHAR;

    return scan_buf;
}

static unsigned char
unescape_char(unsigned char c)
{
    switch (c)
    {
        case 'b':
            return '\b';
        case 'f':
            return '\f';
        case 'n':
            return '\n';
        case 'r':
            return '\r';
        case 't':
            return '\t';
        default:
            return c;
    }
}
