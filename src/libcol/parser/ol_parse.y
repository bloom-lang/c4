%{
/* XXX: should be a pure-parser */
#include <stdio.h>

#include "ol_scan.h"
#include "parser/ast.h"
#include "util/list.h"
#include "util/mem.h"

int yyerror(const char *message);

#define YYERROR_VERBOSE
%}

%union
{
    char *str;
    List *list;
    void *ptr;
}

%start program

%token KEYS DEFINE PROGRAM
%token <str> IDENT ICONST FCONST SCONST

%type <ptr>     program clause define
%type <str>     program_header
%type <list>    program_body int_list ident_list define_schema opt_keys

%%
program: program_header program_body {
    AstProgram *n = ol_alloc(sizeof(*n));
    n->name = $1;
    n->clauses = $2;
    $$ = n;
};

program_header: PROGRAM IDENT ';' { printf("NAME = %s\n", $2); $$ = $2; };

program_body: clause program_body { $$ = NULL; }
| /* EMPTY */ { $$ = NULL; }
;

clause: define { $$ = $1; };
;

define: DEFINE '(' IDENT ',' opt_keys define_schema ')' ';' {
    AstDefine *n = ol_alloc(sizeof(*n));
    n->name = $3;
    n->keys = $5;
    n->schema = $6;
    $$ = n;
};

opt_keys: KEYS '(' int_list ')' ',' { $$ = $3; }
| /* EMPTY */ { $$ = NULL; }
;

define_schema: '{' ident_list '}' { $$ = $2; };

int_list: ICONST { $$ = list_make1((void *) $1); }
| ICONST ',' int_list { $$ = list_append($3, $1); }
;

ident_list: IDENT { $$ = list_make1($1); }
| IDENT ',' ident_list { $$ = list_append($3, $1);}
;
%%

int
yyerror(const char *message)
{
    printf("Parse error: %s\n", message);
    return 1;
}
