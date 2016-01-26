/* tmpl-parser.c
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

#include <glib/gi18n.h>

#include "tmpl-error.h"
#include "tmpl-lexer.h"
#include "tmpl-node.h"
#include "tmpl-parser.h"

struct _TmplParser
{
  GObject               parent_instance;

  TmplNode             *root;
  GInputStream         *stream;
  TmplTemplateLocator  *locator;

  guint                 has_parsed : 1;
};

enum {
  PROP_0,
  PROP_LOCATOR,
  PROP_STREAM,
  LAST_PROP
};

G_DEFINE_TYPE (TmplParser, tmpl_parser, G_TYPE_OBJECT)

static GParamSpec *properties [LAST_PROP];

static void
tmpl_parser_set_stream (TmplParser   *self,
                        GInputStream *stream)
{
  g_assert (TMPL_IS_PARSER (self));
  g_assert (!stream || G_IS_INPUT_STREAM (stream));

  if (stream == NULL)
    {
      g_warning ("TmplParser created without a stream!");
      return;
    }

  g_set_object (&self->stream, stream);
}

static void
tmpl_parser_finalize (GObject *object)
{
  TmplParser *self = (TmplParser *)object;

  g_clear_object (&self->locator);
  g_clear_object (&self->stream);
  g_clear_object (&self->root);

  G_OBJECT_CLASS (tmpl_parser_parent_class)->finalize (object);
}

static void
tmpl_parser_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  TmplParser *self = TMPL_PARSER(object);

  switch (prop_id)
    {
    case PROP_LOCATOR:
      g_value_set_object (value, tmpl_parser_get_locator (self));
      break;

    case PROP_STREAM:
      g_value_set_object (value, self->stream);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
tmpl_parser_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  TmplParser *self = TMPL_PARSER(object);

  switch (prop_id)
    {
    case PROP_LOCATOR:
      tmpl_parser_set_locator (self, g_value_get_object (value));
      break;

    case PROP_STREAM:
      tmpl_parser_set_stream (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
tmpl_parser_class_init (TmplParserClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = tmpl_parser_finalize;
  object_class->get_property = tmpl_parser_get_property;
  object_class->set_property = tmpl_parser_set_property;

  properties [PROP_LOCATOR] =
    g_param_spec_object ("locator",
                         "Locator",
                         "The template locator for resolving includes",
                         TMPL_TYPE_TEMPLATE_LOCATOR,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_STREAM] =
    g_param_spec_object ("stream",
                         "Stream",
                         "The stream to parse",
                         G_TYPE_INPUT_STREAM,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
tmpl_parser_init (TmplParser *self)
{
  self->root = tmpl_node_new ();
}

TmplParser *
tmpl_parser_new (GInputStream *stream)
{
  g_return_val_if_fail (G_IS_INPUT_STREAM (stream), NULL);

  return g_object_new (TMPL_TYPE_PARSER,
                       "stream", stream,
                       NULL);
}

/**
 * tmpl_parser_get_root:
 * @self: A #TmplNode.
 *
 * Gets the root node for the parser.
 *
 * See tmpl_parser_visit_children() to apply a visitor to all nodes created
 * by the parser.
 *
 * Returns: (transfer none): An #TmplNode.
 */
TmplNode *
tmpl_parser_get_root (TmplParser *self)
{
  g_return_val_if_fail (TMPL_IS_PARSER (self), NULL);

  return self->root;
}

gboolean
tmpl_parser_parse (TmplParser    *self,
                   GCancellable  *cancellable,
                   GError       **error)
{
  TmplLexer *lexer;
  GError *local_error = NULL;

  g_return_val_if_fail (TMPL_IS_PARSER (self), FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  if (self->has_parsed)
    {
      g_set_error (error,
                   TMPL_ERROR,
                   TMPL_ERROR_INVALID_STATE,
                   _("%s() may only be called once"),
                   G_STRFUNC);
      return FALSE;
    }

  self->has_parsed = TRUE;

  if (self->stream == NULL)
    {
      g_set_error (error,
                   TMPL_ERROR,
                   TMPL_ERROR_INVALID_STATE,
                   _("Parser does not contain an input stream"));
      return FALSE;
    }

  lexer = tmpl_lexer_new (self->stream, self->locator);
  tmpl_node_accept (self->root, lexer, cancellable, &local_error);
  tmpl_lexer_free (lexer);

  if (local_error != NULL)
    {
      g_propagate_error (error, local_error);
      return FALSE;
    }

  return TRUE;
}

/**
 * tmpl_parser_get_locator:
 * @self: an #TmplParser
 *
 * Gets the template loader used for resolving includes when parsing template
 * files.
 *
 * Includes are performed using the {{include "path"}} token.  The locator can
 * be used to restrict where the templates are located from. By default, the
 * search path is empty, and includes cannot be performed.
 *
 * Returns: (transfer none): A #TmplTemplateLocator.
 */
TmplTemplateLocator *
tmpl_parser_get_locator (TmplParser *self)
{
  g_return_val_if_fail (TMPL_IS_PARSER (self), NULL);

  return self->locator;
}

/**
 * tmpl_parser_set_locator:
 * @self: A #TmplParser
 * @locator: A #TmplTemplateLocator
 *
 * Sets the template locator used to resolve {{include "path"}} directives.
 *
 * See tmpl_parser_get_locator() for more information.
 */
void
tmpl_parser_set_locator (TmplParser          *self,
                         TmplTemplateLocator *locator)
{
  g_return_if_fail (TMPL_IS_PARSER (self));
  g_return_if_fail (!locator || TMPL_IS_TEMPLATE_LOCATOR (locator));

  if (g_set_object (&self->locator, locator))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LOCATOR]);
}
