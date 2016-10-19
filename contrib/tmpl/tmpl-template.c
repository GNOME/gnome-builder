/* tmpl-template.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "tmpl-template"

#include <glib/gi18n.h>
#include <string.h>

#include "tmpl-branch-node.h"
#include "tmpl-condition-node.h"
#include "tmpl-error.h"
#include "tmpl-expr-node.h"
#include "tmpl-iter-node.h"
#include "tmpl-iterator.h"
#include "tmpl-parser.h"
#include "tmpl-scope.h"
#include "tmpl-symbol.h"
#include "tmpl-template.h"
#include "tmpl-text-node.h"
#include "tmpl-util-private.h"

typedef struct
{
  TmplParser          *parser;
  TmplTemplateLocator *locator;
} TmplTemplatePrivate;

typedef struct
{
  TmplTemplate   *self;
  TmplNode       *root;
  GString        *output;
  TmplScope      *scope;
  GError        **error;
  gboolean        result;
} TmplTemplateExpandState;

G_DEFINE_TYPE_WITH_PRIVATE (TmplTemplate, tmpl_template, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_LOCATOR,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static void
tmpl_template_finalize (GObject *object)
{
  TmplTemplate *self = (TmplTemplate *)object;
  TmplTemplatePrivate *priv = tmpl_template_get_instance_private (self);

  g_clear_object (&priv->parser);

  G_OBJECT_CLASS (tmpl_template_parent_class)->finalize (object);
}

static void
tmpl_template_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  TmplTemplate *self = TMPL_TEMPLATE(object);

  switch (prop_id)
    {
    case PROP_LOCATOR:
      g_value_set_object (value, tmpl_template_get_locator (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
tmpl_template_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  TmplTemplate *self = TMPL_TEMPLATE(object);

  switch (prop_id)
    {
    case PROP_LOCATOR:
      tmpl_template_set_locator (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
tmpl_template_class_init (TmplTemplateClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = tmpl_template_finalize;
  object_class->get_property = tmpl_template_get_property;
  object_class->set_property = tmpl_template_set_property;

  properties [PROP_LOCATOR] =
    g_param_spec_object ("locator",
                         "Locator",
                         "The locator used for resolving includes",
                         TMPL_TYPE_TEMPLATE_LOCATOR,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
tmpl_template_init (TmplTemplate *self)
{
}

/**
 * tmpl_template_new:
 * @locator: (nullable): A #TmplTemplateLocator or %NULL.
 *
 * Creates a new #TmplTemplate.
 *
 * If @locator is specified, @locator will be used to resolve include
 * directives when parsing the template.
 *
 * Returns: (transfer full): A #TmplTemplate.
 */
TmplTemplate *
tmpl_template_new (TmplTemplateLocator *locator)
{
  return g_object_new (TMPL_TYPE_TEMPLATE,
                       "locator", locator,
                       NULL);
}

gboolean
tmpl_template_parse_file (TmplTemplate  *self,
                          GFile         *file,
                          GCancellable  *cancellable,
                          GError       **error)
{
  GInputStream *stream;
  gboolean ret = FALSE;

  g_return_val_if_fail (TMPL_IS_TEMPLATE (self), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  stream = (GInputStream *)g_file_read (file, cancellable, error);

  if (stream != NULL)
    {
      ret = tmpl_template_parse (self, stream, cancellable, error);
      g_object_unref (stream);
    }

  return ret;
}

gboolean
tmpl_template_parse_path (TmplTemplate  *self,
                          const gchar   *path,
                          GCancellable  *cancellable,
                          GError       **error)
{
  gboolean ret;
  GFile *file;

  g_return_val_if_fail (TMPL_IS_TEMPLATE (self), FALSE);
  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  file = g_file_new_for_path (path);
  ret = tmpl_template_parse_file (self, file, cancellable, error);
  g_object_unref (file);

  return ret;
}

gboolean
tmpl_template_parse_resource (TmplTemplate  *self,
                              const gchar   *resource_path,
                              GCancellable  *cancellable,
                              GError       **error)
{
  gchar *copied = NULL;
  gboolean ret;
  GFile *file;

  g_return_val_if_fail (TMPL_IS_TEMPLATE (self), FALSE);
  g_return_val_if_fail (resource_path != NULL, FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  if (!g_str_has_prefix (resource_path, "resource://"))
    resource_path = copied = g_strdup_printf ("resource://%s", resource_path);

  file = g_file_new_for_uri (resource_path);
  ret = tmpl_template_parse_file (self, file, cancellable, error);

  g_object_unref (file);
  g_free (copied);

  return ret;
}

gboolean
tmpl_template_parse_string (TmplTemplate  *self,
                            const gchar   *str,
                            GError       **error)
{
  GInputStream *stream;
  gboolean ret;

  g_return_val_if_fail (TMPL_IS_TEMPLATE (self), FALSE);
  g_return_val_if_fail (str, FALSE);

  stream = g_memory_input_stream_new_from_data (g_strdup (str), strlen (str), g_free);
  ret = tmpl_template_parse (self, stream, NULL, error);
  g_object_unref (stream);

  return ret;
}

gboolean
tmpl_template_parse (TmplTemplate  *self,
                     GInputStream  *stream,
                     GCancellable  *cancellable,
                     GError       **error)
{
  TmplTemplatePrivate *priv = tmpl_template_get_instance_private (self);
  TmplParser *parser;
  gboolean ret = FALSE;

  g_return_val_if_fail (TMPL_IS_TEMPLATE (self), FALSE);
  g_return_val_if_fail (G_IS_INPUT_STREAM (stream), FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  parser = tmpl_parser_new (stream);

  tmpl_parser_set_locator (parser, priv->locator);

  if (tmpl_parser_parse (parser, cancellable, error))
    {
      g_set_object (&priv->parser, parser);
      ret = TRUE;
    }

  g_object_unref (parser);

  return ret;
}

static void
value_into_string (const GValue *value,
                   GString      *str)
{
  GValue transform = G_VALUE_INIT;

  g_value_init (&transform, G_TYPE_STRING);

  if (g_value_transform (value, &transform))
    {
      const gchar *tmp;

      if (NULL != (tmp = g_value_get_string (&transform)))
        g_string_append (str, tmp);
    }

  g_value_unset (&transform);
}

static void
tmpl_template_expand_visitor (TmplNode *node,
                              gpointer  user_data)
{
  TmplTemplateExpandState *state = user_data;

  g_assert (TMPL_IS_NODE (node));
  g_assert (state != NULL);

  /* Short cirtcuit if an error occurred */
  if (state->result == FALSE)
    return;

  if (TMPL_IS_TEXT_NODE (node))
    {
      g_string_append (state->output, tmpl_text_node_get_text (TMPL_TEXT_NODE (node)));
    }
  else if (TMPL_IS_EXPR_NODE (node))
    {
      GValue return_value = { 0 };
      TmplExpr *expr;

      expr = tmpl_expr_node_get_expr (TMPL_EXPR_NODE (node));

      if (!tmpl_expr_eval (expr, state->scope, &return_value, state->error))
        {
          state->result = FALSE;
          return;
        }

      value_into_string (&return_value, state->output);
      g_value_unset (&return_value);
    }
  else if (TMPL_IS_BRANCH_NODE (node))
    {
      TmplNode *child;
      GError *local_error = NULL;

      child = tmpl_branch_node_branch (TMPL_BRANCH_NODE (node), state->scope, &local_error);

      if (child != NULL)
        tmpl_node_visit_children (child, tmpl_template_expand_visitor, state);
      else if (local_error != NULL)
        {
          g_propagate_error (state->error, local_error);
          state->result = FALSE;
        }
    }
  else if (TMPL_IS_CONDITION_NODE (node))
    {
      TmplExpr *expr;
      GValue value = G_VALUE_INIT;

      expr = tmpl_condition_node_get_condition (TMPL_CONDITION_NODE (node));

      if (!tmpl_expr_eval (expr, state->scope, &value, state->error))
        {
          state->result = FALSE;
          return;
        }

      if (tmpl_value_as_boolean (&value))
        tmpl_node_visit_children (node, tmpl_template_expand_visitor, state);

      TMPL_CLEAR_VALUE (&value);
    }
  else if (TMPL_IS_ITER_NODE (node))
    {
      const gchar *identifier;
      TmplExpr *expr;
      GValue return_value = G_VALUE_INIT;

      identifier = tmpl_iter_node_get_identifier (TMPL_ITER_NODE (node));

      expr = tmpl_iter_node_get_expr (TMPL_ITER_NODE (node));

      if (!tmpl_expr_eval (expr, state->scope, &return_value, state->error))
        {
          state->result = FALSE;
          return;
        }

      if (tmpl_value_as_boolean (&return_value))
        {
          TmplIterator iter;
          TmplScope *old_scope = state->scope;
          TmplScope *new_scope = tmpl_scope_new_with_parent (old_scope);
          TmplSymbol *symbol;

          state->scope = new_scope;

          symbol = tmpl_scope_get (new_scope, identifier);

          tmpl_iterator_init (&iter, &return_value);

          while (tmpl_iterator_next (&iter))
            {
              GValue value = G_VALUE_INIT;

              tmpl_iterator_get_value (&iter, &value);
              tmpl_symbol_assign_value (symbol, &value);
              TMPL_CLEAR_VALUE (&value);

              tmpl_node_visit_children (node, tmpl_template_expand_visitor, state);

              if (state->result == FALSE)
                break;
            }

          state->scope = old_scope;
          tmpl_scope_unref (new_scope);
        }

      g_value_unset (&return_value);
    }
  else
    {
      g_warning ("Teach me how to expand %s", G_OBJECT_TYPE_NAME (node));
    }
}

/**
 * tmpl_template_expand:
 * @self: A TmplTemplate.
 * @stream: a #GOutputStream to write the results to
 * @scope: (nullable): A #TmplScope containing state for the template, or %NULL.
 * @cancellable: (nullable): An optional cancellable for the operation.
 * @error: A location for a #GError, or %NULL.
 *
 * Expands a template into @stream using the @scope provided.
 *
 * @scope should have all of the variables set that are required to expand
 * the template, or you will get a symbol reference error and %FALSE will
 * be returned.
 *
 * To set a symbol value, get the symbol with tmpl_scope_get() and assign
 * a value using tmpl_scope_assign_value() or similar methods.
 *
 * Returns: %TRUE if successful, otherwise %FALSE and @error is set.
 */
gboolean
tmpl_template_expand (TmplTemplate  *self,
                      GOutputStream *stream,
                      TmplScope     *scope,
                      GCancellable  *cancellable,
                      GError       **error)
{
  TmplTemplatePrivate *priv = tmpl_template_get_instance_private (self);
  TmplTemplateExpandState state = { 0 };
  TmplScope *local_scope = NULL;

  g_return_val_if_fail (TMPL_IS_TEMPLATE (self), FALSE);
  g_return_val_if_fail (G_IS_OUTPUT_STREAM (stream), FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  if (priv->parser == NULL)
    {
      g_set_error (error,
                   TMPL_ERROR,
                   TMPL_ERROR_INVALID_STATE,
                   _("Must parse template before expanding"));
      return FALSE;
    }

  if (scope == NULL)
    scope = local_scope = tmpl_scope_new ();

  state.root = tmpl_parser_get_root (priv->parser);
  state.self = self;
  state.output = g_string_new (NULL);
  state.result = TRUE;
  state.error = error;
  state.scope = scope;

  tmpl_node_visit_children (state.root, tmpl_template_expand_visitor, &state);

  if (state.result != FALSE)
    state.result = g_output_stream_write_all (stream,
                                              state.output->str,
                                              state.output->len,
                                              NULL,
                                              cancellable,
                                              error);

  g_string_free (state.output, TRUE);

  if (local_scope != NULL)
    tmpl_scope_unref (local_scope);

  g_assert (state.result == TRUE || (state.error == NULL || *state.error != NULL));

  return state.result;
}

/**
 * tmpl_template_expand_string:
 * @self: A #TmplTemplate.
 * @scope: (nullable): A #TmplScope or %NULL.
 * @error: A location for a #GError, or %NULL
 *
 * Expands the template and returns the result as a string.
 *
 * Returns: A newly allocated string, or %NULL upon failure.
 */
gchar *
tmpl_template_expand_string (TmplTemplate  *self,
                             TmplScope     *scope,
                             GError       **error)
{
  GOutputStream *stream;
  gchar zero = 0;
  gchar *ret;

  g_return_val_if_fail (TMPL_IS_TEMPLATE (self), NULL);

  stream = g_memory_output_stream_new (NULL, 0, g_realloc, g_free);

  if (!tmpl_template_expand (self, stream, scope, NULL, error) ||
      !g_output_stream_write_all (stream, &zero, 1, NULL, NULL, error) ||
      !g_output_stream_close (stream, NULL, error))

    {
      g_object_unref (stream);

      if (error != NULL && *error == NULL)
        g_set_error (error,
                     G_IO_ERROR,
                     G_IO_ERROR_UNKNOWN,
                     "An unknown error occurred while expanding the template");

      return NULL;
    }

  ret = g_memory_output_stream_steal_data (G_MEMORY_OUTPUT_STREAM (stream));

  g_object_unref (stream);

  return ret;
}

/**
 * tmpl_template_get_locator:
 * @self: A #TmplTemplate
 *
 * Gets the template locator used when resolving template includes.
 *
 * Returns: (transfer none): a #TmplTemplateLocator or %NULL.
 */
TmplTemplateLocator *
tmpl_template_get_locator (TmplTemplate *self)
{
  TmplTemplatePrivate *priv = tmpl_template_get_instance_private (self);

  g_return_val_if_fail (TMPL_IS_TEMPLATE (self), NULL);

  return priv->locator;
}

void
tmpl_template_set_locator (TmplTemplate        *self,
                           TmplTemplateLocator *locator)
{
  TmplTemplatePrivate *priv = tmpl_template_get_instance_private (self);

  g_return_if_fail (TMPL_IS_TEMPLATE (self));
  g_return_if_fail (!locator || TMPL_IS_TEMPLATE_LOCATOR (locator));

  if (g_set_object (&priv->locator, locator))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LOCATOR]);
}
