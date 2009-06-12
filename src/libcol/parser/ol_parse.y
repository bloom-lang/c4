%{
/* XXX: should be a pure-parser */
#include <stdio.h>

#include "ol_scan.h"
#include "util/list.h"

int yyerror(const char *message);

#define YYERROR_VERBOSE
%}

%union
{
    char *str;
    List *list;
    void *ptr;
}

%start input

%token KEYS DEFINE PROGRAM
%token <str> IDENT ICONST FCONST SCONST

%type <str>     program_header
%type <list>    program_body
%type <ptr>     clause

%%
input: program_header program_body {};

program_header: PROGRAM IDENT ';' { printf("NAME = %s\n", $2); $$ = $2; };

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
