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
    bool boolean;
    AstHashVariant hash_v;
}

%start program

%token KEYS DEFINE PROGRAM DELETE OLG_HASH_INSERT OLG_HASH_DELETE
%token <str> IDENT ICONST FCONST SCONST

%type <ptr>     program clause define rule table_ref column_ref
%type <str>     program_header
%type <list>    program_body int_list ident_list define_schema opt_keys
%type <list>    table_ref_list column_ref_list
%type <boolean> opt_delete opt_loc_spec
%type <hash_v>  opt_hash_variant

%%
program: program_header program_body {
    AstProgram *n = ol_alloc0(sizeof(*n));
    n->name = $1;
    n->clauses = $2;
    $$ = n;
};

program_header: PROGRAM IDENT ';' { printf("NAME = %s\n", $2); $$ = $2; };

program_body: clause ';' program_body { $$ = NULL; }
| /* EMPTY */ { $$ = NULL; }
;

clause: define { $$ = $1; };
| rule { $$ = $1; }
;

define: DEFINE '(' IDENT ',' opt_keys define_schema ')' ';' {
    AstDefine *n = ol_alloc0(sizeof(*n));
    n->name = $3;
    n->keys = $5;
    n->schema = $6;

    if (list_length(n->schema) == 0)
        yyerror("empty table schema is not allowed");

    $$ = n;
};

/*
 * Note that we currently equate an empty key list with an absent key list;
 * this is inconsistent with JOL
 */
opt_keys: KEYS '(' int_list ')' ',' { $$ = $3; }
| /* EMPTY */ { $$ = NULL; }
;

define_schema: '{' ident_list '}' { $$ = $2; };

int_list: ICONST ',' int_list { $$ = list_prepend($3, $1); }
| /* EMPTY */ { $$ = NULL; }
;

ident_list: IDENT ',' ident_list { $$ = list_prepend($3, $1);}
| /* EMPTY */ { $$ = NULL; }
;

rule: opt_delete table_ref ':' '-' table_ref_list ';' {
    AstRule *n = ol_alloc0(sizeof(*n));
    n->is_delete = $1;
    n->head = $2;
    n->body = $5;
    $$ = n;
};

opt_delete: DELETE { $$ = true; }
| /* EMPTY */ { $$ = false; }
;

table_ref_list: table_ref ',' table_ref_list { $$ = list_prepend($3, $1); }
| /* EMPTY */ { $$ = NULL; }
;

table_ref: IDENT opt_hash_variant '(' column_ref_list ')' {
    AstTableRef *n = ol_alloc0(sizeof(*n));
    n->name = $1;
    n->hash_variant = $2;
    n->cols = $4;
    $$ = n;
};

opt_hash_variant: OLG_HASH_DELETE { $$ = AST_HASH_DELETE; }
| OLG_HASH_INSERT { $$ = AST_HASH_INSERT; }
| /* EMPTY */ { $$ = AST_HASH_NONE; }
;

column_ref_list: column_ref ',' column_ref_list { $$ = list_prepend($3, $1); }
| /* EMPTY */ { $$ = NULL; }
;

column_ref: opt_loc_spec IDENT {
    AstColumnRef *n = ol_alloc0(sizeof(*n));
    n->has_loc_spec = $1;
    n->name = $2;
    $$ = n;
};

opt_loc_spec: '@' { $$ = true; }
| /* EMPTY */ { $$ = false; }
;

%%

int
yyerror(const char *message)
{
    printf("Parse error: %s\n", message);
    return 1;
}
