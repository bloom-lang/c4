%{
/* XXX: should be a reentrant scanner */
#include <string.h>

#include "col-internal.h"
#include "parser/ast.h"
#include "util/list.h"
#include "util/str.h"

#include "ol_parse.h"
%}

%option 8bit
%option never-interactive batch
%option fast
%option nounput
%option noyywrap

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
