%{
/* XXX: should be a reentrant scanner */
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
self			[,()\[\].;\:\+\-\*\/\%\^\<\>\=]

comment			("//"{non_newline}*)
whitespace		({space}+|{comment})

ident_start		[A-Za-z\200-\377_]
ident_cont		[A-Za-z\200-\377_0-9\$]
identifier		{ident_start}{ident_cont}*

%x comment

%%
"/*"                    { BEGIN(comment); }
<comment>[^*]*          /* eat anything that's not a '*' */
<comment>"*"+[^*/]*     /* eat up '*'s not followed by '/'s */
<comment>"*"+"/"        { BEGIN(INITIAL); }

{whitespace}            /* ignore */
<<EOF>>                 { return OLG_EOF; }

{self}                  { return yytext[0]; }

{identifier} {
    yylval.str = downcase_str(yytext, yyleng);
    return IDENT;
}

%%
