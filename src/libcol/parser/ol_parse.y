%{
/* XXX: should be a pure-parser */
#include "col-internal.h"
#include "ol_scan.h"
#include "parser/parser-internal.h"
#include "util/list.h"

int yyerror(ColParser *context, void *scanner, const char *message);
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
%error-verbose
%parse-param { ColParser *context }
%parse-param { void *scanner }
%lex-param { yyscan_t scanner }

%token KEYS DEFINE PROGRAM DELETE OLG_HASH_INSERT OLG_HASH_DELETE
%token <str> IDENT ICONST FCONST SCONST

%type <ptr>     program clause define rule table_ref column_ref
%type <str>     program_header
%type <list>    program_body opt_int_list int_list ident_list define_schema
%type <list>    opt_keys table_ref_list column_ref_list opt_rule_body
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

program_body: clause ';' program_body { $$ = list_prepend($3, $1); }
| /* EMPTY */ { $$ = NULL; }
;

clause: define { $$ = $1; };
| rule { $$ = $1; }
;

define: DEFINE '(' IDENT ',' opt_keys define_schema ')' {
    AstDefine *n = ol_alloc0(sizeof(*n));
    n->name = $3;
    n->keys = $5;
    n->schema = $6;
    $$ = n;
};

/*
 * Note that we currently equate an empty key list with an absent key list;
 * this is inconsistent with JOL
 */
opt_keys: KEYS '(' opt_int_list ')' ',' { $$ = $3; }
| /* EMPTY */ { $$ = NULL; }
;

define_schema: '{' ident_list '}' { $$ = $2; };

opt_int_list: int_list { $$ = $1; }
| /* EMPTY */ { $$ = NULL; }
;

int_list:
  ICONST { $$ = list_make1((void *) $1); }
| int_list ',' ICONST { $$ = list_append($1, $3); }
;

ident_list:
  IDENT { $$ = list_make1($1); }
| ident_list ',' ICONST { $$ = list_append($1, $3); }
;

rule: opt_delete table_ref opt_rule_body {
    AstRule *n = ol_alloc0(sizeof(*n));
    n->is_delete = $1;
    n->head = $2;
    n->body = $3;
    $$ = n;
};

opt_rule_body: ':' '-' table_ref_list { $$ = $3; }
| /* EMPTY */ { $$ = NULL; }
;

opt_delete: DELETE { $$ = true; }
| /* EMPTY */ { $$ = false; }
;

table_ref_list:
  table_ref { $$ = list_make1($1); }
| table_ref_list ',' table_ref { $$ = list_append($1, $3); }
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

column_ref_list:
  column_ref { $$ = list_make1($1); }
| column_ref_list ',' column_ref { $$ = list_append($1, $3); }
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
yyerror(ColParser *context, void *scanner, const char *message)
{
    printf("Parse error: %s\n", message);
    return 0;   /* return value ignored */
}
