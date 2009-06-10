%{
#include "util/list.h"
%}

%union
{
    char *str;
    List *list;
    void *ptr;
}

%start input
%pure_parser

%token DEFINE PROGRAM OLG_EOF
%token <str> IDENT ICONST FCONST SCONST

%type <str>     program_header
%type <list>    program_body
%type <ptr>     clause rule fact define

%%
input: program_header program_body OLG_EOF {}

program_header: PROGRAM IDENT { $$ = $2; };

program_body: clause program_body { $$ = list_append($2, $1); }
| /* EMPTY */ { $$ = NULL; }
;

clause: rule | fact | define;

rule:;

fact:;

define:;
%%
