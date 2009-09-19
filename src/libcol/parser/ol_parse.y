%{
/* XXX: should be a pure-parser */
#include "col-internal.h"
#include "nodes/makefuncs.h"
/* XXX: see note about #include order in parser.c */
#include "parser/parser-internal.h"
#include "ol_scan.h"
#include "util/list.h"

int yyerror(ColParser *context, void *scanner, const char *message);
static void split_program_clauses(List *clauses, List **defines, List **facts,
                                  List **rules, apr_pool_t *pool);
static void split_rule_body(List *body, List **joins,
                            List **quals, apr_pool_t *pool);
%}

%union
{
    apr_int64_t     ival;
    char           *str;
    List           *list;
    void           *ptr;
    bool            boolean;
    AstHashVariant  hash_v;
}

%start program
%error-verbose
%parse-param { ColParser *context }
%parse-param { void *scanner }
%lex-param { yyscan_t scanner }

%token KEYS DEFINE DELETE NOTIN OL_HASH_INSERT OL_HASH_DELETE
       OL_FALSE OL_TRUE
%token <str> VAR_IDENT TBL_IDENT FCONST SCONST CCONST ICONST

%left OL_EQ OL_NEQ
%nonassoc '>' '<' OL_GTE OL_LTE
%left '+' '-'
%left '*' '/' '%'
%left UMINUS

%type <ptr>     clause define rule table_ref column_ref join_clause
%type <list>    program_body opt_int_list int_list schema_list define_schema
%type <list>    opt_keys column_ref_list opt_rule_body rule_body
%type <ptr>     rule_body_elem qualifier qual_expr expr const_expr op_expr
%type <ptr>     var_expr column_ref_expr rule_prefix schema_elt
%type <str>     bool_const
%type <ival>    iconst_ival
%type <boolean> opt_not opt_delete
%type <hash_v>  opt_hash_variant

%%
program: program_body {
    List *defines;
    List *facts;
    List *rules;

    split_program_clauses($1, &defines, &facts, &rules, context->pool);
    context->result = make_program(defines, facts, rules, context->pool);
};

program_body:
  clause ';' program_body       { $$ = list_prepend($3, $1); }
| /* EMPTY */                   { $$ = list_make(context->pool); }
;

clause:
  define        { $$ = $1; }
| rule          { $$ = $1; }
;

define: DEFINE '(' TBL_IDENT ',' opt_keys define_schema ')' {
    $$ = make_define($3, $5, $6, context->pool);
};

/*
 * Note that we currently equate an empty key list with an absent key list;
 * this is inconsistent with JOL
 */
opt_keys:
  KEYS '(' opt_int_list ')' ',' { $$ = $3; }
| /* EMPTY */ { $$ = NULL; }
;

define_schema: '{' schema_list '}' { $$ = $2; };

opt_int_list:
  int_list      { $$ = $1; }
| /* EMPTY */   { $$ = NULL; }
;

int_list:
  iconst_ival                { $$ = list_make1_int($1, context->pool); }
| int_list ',' iconst_ival   { $$ = list_append_int($1, $3); }
;

/*
 * This is identical to ICONST, except that we convert the string returned
 * by the lexer into an integer. Note that this conversion might fail,
 * because the integer is malformed (e.g. too large).
 */
iconst_ival: ICONST {
    apr_int64_t ival;
    char *endptr;

    errno = 0;
    ival = strtol($1, &endptr, 10);
    if (errno != 0 || endptr[0] != '\0')
        FAIL();

    $$ = ival;
};

schema_list:
  schema_elt                  { $$ = list_make1($1, context->pool); }
| schema_list ',' schema_elt  { $$ = list_append($1, $3); }
;

schema_elt:
  '@' TBL_IDENT               { $$ = make_schema_elt($2, true, context->pool); }
| TBL_IDENT                   { $$ = make_schema_elt($1, false, context->pool); }
;

rule: rule_prefix opt_rule_body {
    AstRule *rule = (AstRule *) $1;

    if ($2 == NULL)
    {
        /* A rule without a body is actually a fact */
        if (rule->name != NULL)
            ERROR("Cannot assign a name to facts");
        if (rule->is_delete)
            ERROR("Cannot specify \"delete\" in a fact");

        $$ = make_fact(rule->head, context->pool);
    }
    else
    {
        List *joins;
        List *quals;

        split_rule_body($2, &joins, &quals, context->pool);
        rule->joins = joins;
        rule->quals = quals;
        $$ = rule;
    }
};

rule_prefix:
  TBL_IDENT opt_delete table_ref { $$ = make_rule($1, $2, false, $3, NULL, NULL, context->pool); }
| DELETE table_ref               { $$ = make_rule(NULL, true, false, $2, NULL, NULL, context->pool); }
| table_ref                      { $$ = make_rule(NULL, false, false, $1, NULL, NULL, context->pool); }
;

opt_delete:
  DELETE        { $$ = true; }
| /* EMPTY */   { $$ = false; }
;

opt_rule_body:
  ':' '-' rule_body     { $$ = $3; }
| /* EMPTY */           { $$ = NULL; }
;

rule_body:
  rule_body_elem                { $$ = list_make1($1, context->pool); }
| rule_body ',' rule_body_elem  { $$ = list_append($1, $3); }
;

rule_body_elem:
  join_clause
| qualifier
;

join_clause: opt_not TBL_IDENT opt_hash_variant '(' column_ref_list ')' {
    AstTableRef *ref = make_table_ref($2, $5, context->pool);
    $$ = make_join_clause(ref, $1, $3, context->pool);
};

opt_not:
  NOTIN                 { $$ = true; }
| /* EMPTY */           { $$ = false; }
;

opt_hash_variant:
  OL_HASH_DELETE       { $$ = AST_HASH_DELETE; }
| OL_HASH_INSERT       { $$ = AST_HASH_INSERT; }
| /* EMPTY */          { $$ = AST_HASH_NONE; }
;

table_ref: TBL_IDENT '(' column_ref_list ')' { $$ = make_table_ref($1, $3, context->pool); };

qualifier: qual_expr { $$ = make_qualifier($1, context->pool); };

expr:
  op_expr
| const_expr
| var_expr
;

op_expr:
  expr '+' expr         { $$ = make_ast_op_expr($1, $3, AST_OP_PLUS, context->pool); }
| expr '-' expr         { $$ = make_ast_op_expr($1, $3, AST_OP_MINUS, context->pool); }
| expr '*' expr         { $$ = make_ast_op_expr($1, $3, AST_OP_TIMES, context->pool); }
| expr '/' expr         { $$ = make_ast_op_expr($1, $3, AST_OP_DIVIDE, context->pool); }
| expr '%' expr         { $$ = make_ast_op_expr($1, $3, AST_OP_MODULUS, context->pool); }
| '-' expr              { $$ = make_ast_op_expr($2, NULL, AST_OP_UMINUS, context->pool); }
| qual_expr             { $$ = $1; }
;

qual_expr:
  expr '<' expr         { $$ = make_ast_op_expr($1, $3, AST_OP_LT, context->pool); }
| expr '>' expr         { $$ = make_ast_op_expr($1, $3, AST_OP_GT, context->pool); }
| expr OL_LTE expr      { $$ = make_ast_op_expr($1, $3, AST_OP_LTE, context->pool); }
| expr OL_GTE expr      { $$ = make_ast_op_expr($1, $3, AST_OP_GTE, context->pool); }
| expr OL_EQ expr       { $$ = make_ast_op_expr($1, $3, AST_OP_EQ, context->pool); }
| expr OL_NEQ expr      { $$ = make_ast_op_expr($1, $3, AST_OP_NEQ, context->pool); }
;

const_expr:
  bool_const { $$ = make_ast_const_expr(AST_CONST_BOOL, $1, context->pool); }
| ICONST { $$ = make_ast_const_expr(AST_CONST_INT, $1, context->pool); }
| FCONST { $$ = make_ast_const_expr(AST_CONST_DOUBLE, $1, context->pool); }
| SCONST { $$ = make_ast_const_expr(AST_CONST_STRING, $1, context->pool); }
| CCONST { $$ = make_ast_const_expr(AST_CONST_CHAR, $1, context->pool); }
;

bool_const:
  OL_TRUE       { $$ = apr_pstrdup(context->pool, "true"); }
| OL_FALSE      { $$ = apr_pstrdup(context->pool, "false"); }
;

var_expr: VAR_IDENT { $$ = make_ast_var_expr($1, TYPE_INVALID, context->pool); };

column_ref_list:
  column_ref { $$ = list_make1($1, context->pool); }
| column_ref_list ',' column_ref { $$ = list_append($1, $3); }
;

column_ref:
  column_ref_expr   { $$ = make_column_ref($1, context->pool); }
;

column_ref_expr:
  const_expr
| var_expr
| op_expr
;

%%

int
yyerror(ColParser *context, void *scanner, const char *message)
{
    printf("Parse error: %s\n", message);
    return 0;   /* return value ignored */
}

static void
split_program_clauses(List *clauses, List **defines,
                      List **facts, List **rules, apr_pool_t *pool)
{
    ListCell *lc;

    *defines = list_make(pool);
    *facts = list_make(pool);
    *rules = list_make(pool);

    foreach (lc, clauses)
    {
        ColNode *node = (ColNode *) lc_ptr(lc);

        switch (node->kind)
        {
            case AST_DEFINE:
                list_append(*defines, node);
                break;

            case AST_FACT:
                list_append(*facts, node);
                break;

            case AST_RULE:
                list_append(*rules, node);
                break;

            default:
                ERROR("Unexpected node kind: %d", (int) node->kind);
        }
    }
}

/*
 * Split the rule body into join clauses and qualifiers, and store each in
 * its own list for subsequent ease of processing. Note that this implies
 * that the relative order of join clauses and qualifiers is not
 * semantically significant.
 */
static void
split_rule_body(List *body, List **joins, List **quals, apr_pool_t *pool)
{
    ListCell *lc;

    *joins = list_make(pool);
    *quals = list_make(pool);

    foreach (lc, body)
    {
        ColNode *node = (ColNode *) lc_ptr(lc);

        switch (node->kind)
        {
            case AST_JOIN_CLAUSE:
                list_append(*joins, node);
                break;

            case AST_QUALIFIER:
                list_append(*quals, node);
                break;

            default:
                ERROR("Unexpected node kind: %d", (int) node->kind);
        }
    }
}
