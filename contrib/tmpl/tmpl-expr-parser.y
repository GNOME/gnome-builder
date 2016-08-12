%pure-parser
%name-prefix "tmpl_expr_parser_"
%defines
%error-verbose
%parse-param { TmplExprParser *parser }
%lex-param { void *scanner }

%{
# include <glib/gprintf.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>

# include "tmpl-expr.h"
# include "tmpl-expr-private.h"
# include "tmpl-expr-parser-private.h"

#pragma GCC diagnostic ignored "-Wswitch-default"
%}

%union {
  TmplExpr *a;         /* ast node */
  double d;            /* number */
  char *s;             /* symbol/string */
  GPtrArray *sl;       /* symlist */
  TmplExprBuiltin fn;  /* builtin call */
  int b;               /* boolean */
  int cmp;             /* comparison */
}

%{
int tmpl_expr_parser_lex (YYSTYPE *, void *scanner);

void
tmpl_expr_parser_error (TmplExprParser *parser,
                        const gchar    *message)
{
  g_assert (parser != NULL);
  g_assert (message != NULL);

  g_clear_pointer (&parser->ast, tmpl_expr_unref);

  g_free (parser->error_str);
  parser->error_str = g_strdup (message);
}

# define scanner parser->scanner
%}

%token <s> REQUIRE VERSION

%token <b> BOOL
%token <d> NUMBER
%token <s> NAME STRING_LITERAL
%token <fn> BUILTIN
%token EOL

%token IF THEN ELSE WHILE DO FUNC

%nonassoc <cmp> CMP
%right '='
%left '+' '-'
%left '*' '/'

%nonassoc '|' UMINUS

%type <a> exp stmt list explist
%type <sl> symlist

%start expr

%%

expr: /* nothing */ EOL {
    parser->ast = NULL;
    YYACCEPT;
  }
  | stmt EOL {
    parser->ast = $1;
    YYACCEPT;
  }
  | FUNC NAME '(' symlist ')' '=' list EOL {
    /* todo: add ast node to define the expr on the scope
     * when evaluated.
     */
    //tmpl_scope_add_user_func (parser->scope, $2, $4, $7);
    parser->ast = NULL;
    YYACCEPT;
  }
  | FUNC NAME '(' ')' '=' list EOL {
    /* todo: add ast node to define the expr on the scope
     * when evaluated.
     */
    //tmpl_scope_add_user_func (parser->scope, $2, NULL, $6);
    parser->ast = NULL;
    YYACCEPT;
  }
;

stmt: IF exp THEN list {
    $$ = tmpl_expr_new_flow (TMPL_EXPR_IF, $2, $4, NULL);
  }
  | IF exp THEN list ELSE list {
    $$ = tmpl_expr_new_flow (TMPL_EXPR_IF, $2, $4, $6);
  }
  | WHILE exp DO list {
    $$ = tmpl_expr_new_flow (TMPL_EXPR_WHILE, $2, $4, NULL);
  }
  | exp
;

list: /* nothing */ { $$ = NULL; }
  | stmt ';' list {
    if ($3 == NULL)
      $$ = $1;
    else
      $$ = tmpl_expr_new_simple (TMPL_EXPR_STMT_LIST, $1, $3);
  }
;

exp: exp CMP exp {
    $$ = tmpl_expr_new_simple ($2, $1, $3);
  }
  | exp '+' exp {
    $$ = tmpl_expr_new_simple (TMPL_EXPR_ADD, $1, $3);
  }
  | exp '-' exp {
    $$ = tmpl_expr_new_simple (TMPL_EXPR_SUB, $1, $3);
  }
  | exp '*' exp {
    $$ = tmpl_expr_new_simple (TMPL_EXPR_MUL, $1, $3);
  }
  | exp '/' exp {
    $$ = tmpl_expr_new_simple (TMPL_EXPR_DIV, $1, $3);
  }
  | '(' exp ')' {
    $$ = $2;
  }
  | '-' exp %prec UMINUS {
    $$ = tmpl_expr_new_simple (TMPL_EXPR_UNARY_MINUS, $2, NULL);
  }
  | NUMBER {
    $$ = tmpl_expr_new_number ($1);
  }
  | BOOL {
    $$ = tmpl_expr_new_boolean ($1);
  }
  | STRING_LITERAL {
    $$ = tmpl_expr_new_string ($1+1, strlen($1) - 2);
  }
  | NAME {
    $$ = tmpl_expr_new_symbol_ref ($1);
  }
  | NAME '=' exp {
    $$ = tmpl_expr_new_symbol_assign ($1, $3);
  }
  | exp '.' NAME '(' ')' {
    $$ = tmpl_expr_new_gi_call ($1, $3, NULL);
  }
  | exp '.' NAME '(' explist ')' {
    $$ = tmpl_expr_new_gi_call ($1, $3, $5);
  }
  | exp '.' NAME {
    $$ = tmpl_expr_new_getattr ($1, $3);
  }
  | exp '.' NAME '=' exp {
    $$ = tmpl_expr_new_setattr ($1, $3, $5);
  }
  | BUILTIN '(' explist ')' {
    $$ = tmpl_expr_new_fn_call ($1, $3);
  }
  | NAME '(' explist ')' {
    $$ = tmpl_expr_new_user_fn_call ($1, $3);
  }
  | NAME '(' ')' {
    $$ = tmpl_expr_new_user_fn_call ($1, NULL);
  }
  | '!' exp {
    $$ = tmpl_expr_new_invert_boolean ($2);
  }
  | REQUIRE NAME {
    $$ = tmpl_expr_new_require ($2, NULL);
  }
  | REQUIRE NAME VERSION STRING_LITERAL {
    char *vstr = g_strndup ($4+1, strlen($4)-2);
    $$ = tmpl_expr_new_require ($2, vstr);
    g_free (vstr);
  }
;

explist: exp
  | exp ',' explist {
    $$ = tmpl_expr_new_simple (TMPL_EXPR_STMT_LIST, $1, $3);
  }
;

symlist: NAME {
    $$ = g_ptr_array_new_with_free_func (g_free);
    g_ptr_array_add ($$, $1);
  }
  | NAME ',' symlist {
    g_ptr_array_insert ($3, 0, $1);
  }
;

