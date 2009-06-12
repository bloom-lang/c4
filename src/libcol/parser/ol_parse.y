%{
/* XXX: should be a pure-parser */
#include <stdio.h>

#include "scan.h"
#include "util/list.h"

int yyerror(const char *message);
%}

%union
{
    char *str;
    List *list;
    void *ptr;
}

%start input

%token DEFINE PROGRAM OLG_EOF
%token <str> IDENT ICONST FCONST SCONST

%type <str>     program_header
%type <list>    program_body
%type <ptr>     clause

%%
input: program_header program_body OLG_EOF {};

program_header: PROGRAM IDENT { $$ = $2; };

program_body: clause program_body { $$ = NULL; }
| /* EMPTY */ { $$ = NULL; }
;

clause: IDENT { $$ = $1; };
%%

int
yyerror(const char *message)
{
    printf("Parse error: %s\n", message);
    return 1;
}
