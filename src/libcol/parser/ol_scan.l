%{
#include <string.h>

#include "col-internal.h"
#include "parser/parser-internal.h"
#include "util/str.h"

#include "ol_parse.h"
%}

%option 8bit
%option never-interactive batch
%option fast
%option nounput
%option noyywrap
%option reentrant
%option extra-type="ColParser *"

digit           [0-9]
space           [ \t\n\r\f]
newline         [\n\r]
non_newline     [^\n\r]

/* "self" is the set of characters that are returned to the parser as-is. */
self			[,()\[\].;\:\+\-\*\/\%\^\<\>\=@]

comment			("//"{non_newline}*)
whitespace		({space}+|{comment})

/* No support for Unicode identifiers */
ident_start		[A-Za-z_]
ident_cont		[A-Za-z_0-9]
identifier		{ident_start}{ident_cont}*

%x comment

%%
"/*"                    { BEGIN(comment); }
<comment>[^*]*          /* eat anything that's not a '*' */
<comment>"*"+[^*/]*     /* eat up '*'s not followed by '/'s */
<comment>"*"+"/"        { BEGIN(INITIAL); }

{whitespace}            /* ignore */
<INITIAL><<EOF>>        { yyterminate(); }

{self}                  { return yytext[0]; }

"#delete"               { return OLG_HASH_DELETE; }
"#insert"               { return OLG_HASH_INSERT; }

{identifier} {
    yylval.str = downcase_str(yytext, yyleng);

    if (strcmp(yylval.str, "delete") == 0)
        return DELETE;
    if (strcmp(yylval.str, "define") == 0)
        return DEFINE;
    if (strcmp(yylval.str, "keys") == 0)
        return KEYS;
    if (strcmp(yylval.str, "program") == 0)
        return PROGRAM;

    return IDENT;
}

%%

/*
 * Setup an input buffer for Flex to scan. This is defined here mostly so
 * that we can use the YY_END_OF_BUFFER_CHAR #define, which Flex helpfully
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
