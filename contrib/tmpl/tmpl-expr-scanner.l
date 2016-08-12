%option reentrant
%option prefix="tmpl_expr_parser_"
%option bison-bridge
%option noyywrap
%option yylineno
%option nodefault

%option header-file="tmpl-expr-scanner.h"
%option outfile="tmpl-expr-scanner.c"

%{
# include "tmpl-error.h"
# include "tmpl-expr-private.h"
# include "tmpl-expr-parser-private.h"
# include "tmpl-expr-parser.h"

#pragma GCC diagnostic ignored "-Wswitch-default"
#pragma GCC diagnostic ignored "-Wunused-function"
%}

%option extra-type="TmplExprParser *"

 /* float exponent */
EXP ([Ee][-+]?[0-9]+)

%%

%{
TmplExprParser *parser = yyextra;
%}

 /* single character ops */
"+" |
"-" |
"*" |
"/" |
"=" |
"," |
"." |
";" |
"!" |
"(" |
")" { return yytext [0]; }

 /* comparison ops */
">"  { yylval->cmp = TMPL_EXPR_GT;  return CMP; }
"<"  { yylval->cmp = TMPL_EXPR_LT;  return CMP; }
"!=" { yylval->cmp = TMPL_EXPR_NE;  return CMP; }
"==" { yylval->cmp = TMPL_EXPR_EQ;  return CMP; }
">=" { yylval->cmp = TMPL_EXPR_GTE; return CMP; }
"<=" { yylval->cmp = TMPL_EXPR_LTE; return CMP; }
"||" { yylval->cmp = TMPL_EXPR_OR; return CMP; }
"&&" { yylval->cmp = TMPL_EXPR_AND; return CMP; }

 /* keywords */
"if"       { return IF; }
"then"     { return THEN; }
"else"     { return ELSE; }
"while"    { return WHILE; }
"do"       { return DO; }
"func"     { return FUNC; }
"require"  { return REQUIRE; }
"version"  { return VERSION; }

"true"     { yylval->b = 1; return BOOL; }
"false"    { yylval->b = 0; return BOOL; }

 /* builtin functions */
"ceil"  { yylval->fn = TMPL_EXPR_BUILTIN_CEIL; return BUILTIN; }
"floor" { yylval->fn = TMPL_EXPR_BUILTIN_FLOOR; return BUILTIN; }
"hex"   { yylval->fn = TMPL_EXPR_BUILTIN_HEX; return BUILTIN; }
"log"   { yylval->fn = TMPL_EXPR_BUILTIN_LOG; return BUILTIN; }
"print" { yylval->fn = TMPL_EXPR_BUILTIN_PRINT; return BUILTIN; }
"repr"  { yylval->fn = TMPL_EXPR_BUILTIN_REPR; return BUILTIN; }
"sqrt"  { yylval->fn = TMPL_EXPR_BUILTIN_SQRT; return BUILTIN; }

 /* string literals */
L?\"(\\.|[^\\"])*\" { yylval->s = yytext; return STRING_LITERAL; }

 /* names */
[a-zA-Z_][a-zA-Z0-9_]* {
  yylval->s = g_strdup (yytext);
  return NAME;
}

[0-9]+"."[0-9]*{EXP}? |
"."?[0-9]+{EXP}? { yylval->d = atof(yytext); return NUMBER; }

"//".*
[ \t] /* ignore whitespace */
\\n     { printf ("c> "); }
"\n"    { return EOL; }
<<EOF>> { parser->reached_eof = TRUE; return EOL; }
.       {
          g_free (parser->error_str);
          parser->error_str = g_strdup (yytext);
          parser->error_line = yylineno;
        }

%%

void
tmpl_expr_parser_init_scanner (TmplExprParser *parser)
{
  g_assert (parser != NULL);

  yylex_init (&parser->scanner);
  yyset_extra (parser, parser->scanner);
}

void
tmpl_expr_parser_destroy_scanner (TmplExprParser *parser)
{
  g_assert (parser != NULL);

  yylex_destroy (parser->scanner);
}

void
tmpl_expr_parser_flush (TmplExprParser *parser)
{
  struct yyguts_t *yyg;

  g_assert (parser != NULL);

  yyg = (struct yyguts_t *)parser->scanner;
  tmpl_expr_parser__flush_buffer (YY_CURRENT_BUFFER, parser->scanner);
}

gboolean
tmpl_expr_parser_parse_string (TmplExprParser  *self,
                               const gchar    *input,
                               GError        **error)
{
  YY_BUFFER_STATE buf;
  gint ret;

  g_return_val_if_fail (self != NULL, FALSE);

  buf = tmpl_expr_parser__scan_string (input, self->scanner);
  ret = tmpl_expr_parser_parse (self);
  tmpl_expr_parser__delete_buffer (buf, self->scanner);

  if (self->error_str)
    {
      g_set_error (error,
                   TMPL_ERROR,
                   TMPL_ERROR_SYNTAX_ERROR,
                   "%s", self->error_str);
      return FALSE;
    }

  if (ret == 0)
    return TRUE;

  g_set_error (error,
               TMPL_ERROR,
               TMPL_ERROR_SYNTAX_ERROR,
               "Failed to parse expression");

  return FALSE;
}

gboolean
tmpl_expr_parser_init (TmplExprParser  *self,
                       GError        **error)
{
  g_return_val_if_fail (self != NULL, FALSE);

  memset (self, 0, sizeof *self);
  tmpl_expr_parser_init_scanner (self);

  return TRUE;
}

void
tmpl_expr_parser_destroy (TmplExprParser *self)
{
  if (self != NULL)
    {
      tmpl_expr_parser_destroy_scanner (self);
      g_clear_pointer (&self->ast, tmpl_expr_unref);
      g_clear_pointer (&self->error_str, g_free);
    }
}
