%{
#include "y.tab.h"
%}

%option 8bit
%option never-interactive batch
%option reentrant
%option fast
%option nounput
%option noyywrap

digit           [0-9]
space           [ \t\n\r\f]
newline         [\n\r]
non_newline     [^\n\r]

comment			("//"{non_newline}*)
whitespace		({space}+|{comment})


%x comment

%%
"/*"                    BEGIN(comment)
<comment>[^*]*          /* eat anything that's not a '*' */
<comment>"*"+[^*/]*     /* eat up '*'s not followed by '/'s */
<comment>"*"+"/"        BEGIN(INITIAL)

{whitespace}            /* ignore */
%%
