%{
/* XXX: should be a pure-parser */
#include "col-internal.h"
/* XXX: see note about #include order in parser.c */
#include "parser/parser-internal.h"
#include "ol_scan.h"
#include "util/list.h"

int yyerror(ColParser *context, void *scanner, const char *message);
static AstTableRef *make_table_ref(ColParser *context, char *name, List *cols);
static AstOpExpr *make_op_expr(ColParser *context, AstNode *lhs,
                               AstNode *rhs, AstOperKind op_kind);
static AstColumnRef *make_column_ref(ColParser *context, bool has_loc_spec,
                                     AstNode *expr);
static AstRule *make_rule(ColParser *context, char *name,
                          bool is_delete, AstTableRef *head);

#define parser_alloc(sz)        apr_pcalloc(context->pool, (sz))
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

%token KEYS DEFINE PROGRAM DELETE NOTIN OL_HASH_INSERT OL_HASH_DELETE
       OL_ASSIGN OL_FALSE OL_TRUE
%token <str> IDENT FCONST SCONST
%token <ival> ICONST

%nonassoc OL_ASSIGN
%left OL_EQ OL_NEQ
%nonassoc '>' '<' OL_GTE OL_LTE
%left '+' '-'
%left '*' '/' '%'
%left UMINUS

%type <ptr>     clause define rule table_ref column_ref join_clause
%type <str>     program_header
%type <list>    program_body opt_int_list int_list ident_list define_schema
%type <list>    opt_keys column_ref_list opt_rule_body rule_body
%type <ptr>     rule_body_elem predicate pred_expr expr const_expr op_expr
%type <ptr>     var_expr column_ref_expr rule_prefix
%type <boolean> opt_not bool_const opt_delete
%type <hash_v>  opt_hash_variant

%%
program: program_header program_body {
    AstProgram *n = parser_alloc(sizeof(*n));
    n->node.kind = AST_PROGRAM;
    n->name = $1;
    n->clauses = $2;
    context->result = n;
};

program_header: PROGRAM IDENT ';' { $$ = $2; };

program_body:
  clause ';' program_body       { $$ = list_prepend($3, $1); }
| /* EMPTY */                   { $$ = list_make(context->pool); }
;

clause:
  define        { $$ = $1; }
| rule          { $$ = $1; }
;

define: DEFINE '(' IDENT ',' opt_keys define_schema ')' {
    AstDefine *n = parser_alloc(sizeof(*n));
    n->node.kind = AST_DEFINE;
    n->name = $3;
    n->keys = $5;
    n->schema = $6;
    $$ = n;
};

/*
 * Note that we currently equate an empty key list with an absent key list;
 * this is inconsistent with JOL
 */
opt_keys:
  KEYS '(' opt_int_list ')' ',' { $$ = $3; }
| /* EMPTY */ { $$ = NULL; }
;

define_schema: '{' ident_list '}' { $$ = $2; };

opt_int_list:
  int_list      { $$ = $1; }
| /* EMPTY */   { $$ = NULL; }
;

int_list:
  ICONST                { $$ = list_make1_int($1, context->pool); }
| int_list ',' ICONST   { $$ = list_append_int($1, $3); }
;

ident_list:
  IDENT                 { $$ = list_make1($1, context->pool); }
| ident_list ',' IDENT  { $$ = list_append($1, $3); }
;

rule: rule_prefix opt_rule_body {
    AstRule *rule = (AstRule *) $1;

    if ($2 == NULL)
    {
        /* A rule without a body is actually a fact */
        AstFact *n = parser_alloc(sizeof(*n));
        n->node.kind = AST_FACT;
        n->head = rule->head;

        if (rule->name != NULL)
            ERROR("Cannot assign a name to facts");
        if (rule->is_delete)
            ERROR("Cannot specify \"delete\" in a fact");

        $$ = n;
    }
    else
    {
        rule->body = $2;
        $$ = rule;
    }
};

rule_prefix:
  IDENT opt_delete table_ref    { $$ = make_rule(context, $1, $2, $3); }
| DELETE table_ref              { $$ = make_rule(context, NULL, true, $2); }
| table_ref                     { $$ = make_rule(context, NULL, false, $1); }
;

opt_delete:
DELETE { $$ = true; }
| { $$ = false; }
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
| predicate
;

join_clause: opt_not IDENT opt_hash_variant '(' column_ref_list ')' {
    AstJoinClause *n = parser_alloc(sizeof(*n));
    n->node.kind = AST_JOIN_CLAUSE;
    n->not = $1;
    n->hash_variant = $3;
    n->ref = make_table_ref(context, $2, $5);
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

table_ref: IDENT '(' column_ref_list ')' {
    $$ = make_table_ref(context, $1, $3);
};

/* XXX: Temp hack to workaround parser issues */
predicate: '^' pred_expr {
    AstPredicate *n = parser_alloc(sizeof(*n));
    n->node.kind = AST_PREDICATE;
    n->expr = $2;
    $$ = n;
};

expr:
  op_expr
| const_expr
| var_expr
;

op_expr:
  expr '+' expr         { $$ = make_op_expr(context, $1, $3, AST_OP_PLUS); }
| expr '-' expr         { $$ = make_op_expr(context, $1, $3, AST_OP_MINUS); }
| expr '*' expr         { $$ = make_op_expr(context, $1, $3, AST_OP_TIMES); }
| expr '/' expr         { $$ = make_op_expr(context, $1, $3, AST_OP_DIVIDE); }
| expr '%' expr         { $$ = make_op_expr(context, $1, $3, AST_OP_MODULUS); }
| '-' expr              { $$ = make_op_expr(context, $2, NULL, AST_OP_UMINUS); }
| pred_expr             { $$ = $1; }
;

/*
 * Note that we treat assignment as a predicate in the parser, because it
 * would be painful to teach Bison how to tell the difference. Instead, we
 * distinguish between assigments and predicates in the semantic analysis
 * phase, and construct an AstAssign node there.
 */
pred_expr:
  expr '<' expr         { $$ = make_op_expr(context, $1, $3, AST_OP_LT); }
| expr '>' expr         { $$ = make_op_expr(context, $1, $3, AST_OP_GT); }
| expr OL_LTE expr      { $$ = make_op_expr(context, $1, $3, AST_OP_LTE); }
| expr OL_GTE expr      { $$ = make_op_expr(context, $1, $3, AST_OP_GTE); }
| expr OL_EQ expr       { $$ = make_op_expr(context, $1, $3, AST_OP_EQ); }
| expr OL_NEQ expr      { $$ = make_op_expr(context, $1, $3, AST_OP_NEQ); }
| column_ref OL_ASSIGN expr { $$ = make_op_expr(context, $1, $3, AST_OP_ASSIGN); }
;

const_expr:
  ICONST
{
    AstConstExprInt *n = parser_alloc(sizeof(*n));
    n->node.kind = AST_CONST_EXPR_INT;
    n->val = $1;
    $$ = n;
}
| bool_const
{
    AstConstExprBool *n = parser_alloc(sizeof(*n));
    n->node.kind = AST_CONST_EXPR_INT;
    n->val = $1;
    $$ = n;
}
| FCONST
{
    AstConstExprDouble *n = parser_alloc(sizeof(*n));
    n->node.kind = AST_CONST_EXPR_DOUBLE;
    n->val = $1;
    $$ = n;
}
| SCONST
{
    AstConstExprString *n = parser_alloc(sizeof(*n));
    n->node.kind = AST_CONST_EXPR_STRING;
    n->val = $1;
    $$ = n;
}
;

bool_const:
  OL_TRUE       { $$ = true; }
| OL_FALSE      { $$ = false; }
;

var_expr: IDENT {
    AstVarExpr *n = parser_alloc(sizeof(*n));
    n->node.kind = AST_VAR_EXPR;
    n->name = $1;
    $$ = n;
};

column_ref_list:
  column_ref { $$ = list_make1($1, context->pool); }
| column_ref_list ',' column_ref { $$ = list_append($1, $3); }
;

column_ref:
  '@' column_ref_expr   { $$ = make_column_ref(context, true, $2); }
| column_ref_expr       { $$ = make_column_ref(context, false, $1); }
;

column_ref_expr:
  const_expr
| var_expr
;

%%

int
yyerror(ColParser *context, void *scanner, const char *message)
{
    printf("Parse error: %s\n", message);
    return 0;   /* return value ignored */
}

static AstTableRef *
make_table_ref(ColParser *context, char *name, List *cols)
{
    AstTableRef *result = apr_pcalloc(context->pool, sizeof(*result));
    result->node.kind = AST_TABLE_REF;
    result->name = name;
    result->cols = cols;
    return result;
}

static AstOpExpr *
make_op_expr(ColParser *context, AstNode *lhs, AstNode *rhs, AstOperKind op_kind)
{
    AstOpExpr *result = apr_pcalloc(context->pool, sizeof(*result));
    result->node.kind = AST_OP_EXPR;
    result->lhs = lhs;
    result->rhs = rhs;
    result->op_kind = op_kind;
    return result;
}

static AstColumnRef *
make_column_ref(ColParser *context, bool has_loc_spec, AstNode *expr)
{
    AstColumnRef *result = apr_pcalloc(context->pool, sizeof(*result));
    result->node.kind = AST_COLUMN_REF;
    result->has_loc_spec = has_loc_spec;
    result->expr = expr;
    return result;
}

static AstRule *
make_rule(ColParser *context, char *name, bool is_delete, AstTableRef *head)
{
    AstRule *result = apr_pcalloc(context->pool, sizeof(*result));
    result->name = name;
    result->is_delete = is_delete;
    result->head = head;
    return result;
}
