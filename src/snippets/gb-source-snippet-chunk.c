/* gb-source-snippet-chunk.c
 *
 * Copyright (C) 2013 Christian Hergert <christian@hergert.me>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>

#include "gb-source-snippet-chunk.h"

G_DEFINE_TYPE (GbSourceSnippetChunk, gb_source_snippet_chunk, G_TYPE_OBJECT)

struct _GbSourceSnippetChunkPrivate
{
  GbSourceSnippetContext *context;
  guint                   context_changed_handler;
  gint                    tab_stop;
  gchar                  *spec;
  gchar                  *text;
  gboolean                text_set;
};

enum {
  PROP_0,
  PROP_CONTEXT,
  PROP_SPEC,
  PROP_TAB_STOP,
  PROP_TEXT,
  PROP_TEXT_SET,
  LAST_PROP
};

static GParamSpec *gParamSpecs[LAST_PROP];

GbSourceSnippetChunk *
gb_source_snippet_chunk_new (void)
{
  return g_object_new (GB_TYPE_SOURCE_SNIPPET_CHUNK, NULL);
}

GbSourceSnippetChunk *
gb_source_snippet_chunk_copy (GbSourceSnippetChunk *chunk)
{
  GbSourceSnippetChunk *ret;

  g_return_val_if_fail (GB_IS_SOURCE_SNIPPET_CHUNK (chunk), NULL);

  ret = g_object_new (GB_TYPE_SOURCE_SNIPPET_CHUNK,
                      "spec", chunk->priv->spec,
                      "tab-stop", chunk->priv->tab_stop,
                      NULL);

  return ret;
}

static void
on_context_changed (GbSourceSnippetContext *context,
                    GbSourceSnippetChunk   *chunk)
{
  GbSourceSnippetChunkPrivate *priv;
  gchar *text;

  g_return_if_fail (GB_IS_SOURCE_SNIPPET_CHUNK (chunk));
  g_return_if_fail (GB_IS_SOURCE_SNIPPET_CONTEXT (context));

  priv = chunk->priv;

  if (!priv->text_set)
    {
      text = gb_source_snippet_context_expand (context, priv->spec);
      gb_source_snippet_chunk_set_text (chunk, text);
      g_free (text);
    }
}

GbSourceSnippetContext *
gb_source_snippet_chunk_get_context (GbSourceSnippetChunk *chunk)
{
  g_return_val_if_fail (GB_IS_SOURCE_SNIPPET_CHUNK (chunk), NULL);

  return chunk->priv->context;
}

void
gb_source_snippet_chunk_set_context (GbSourceSnippetChunk   *chunk,
                                     GbSourceSnippetContext *context)
{
  GbSourceSnippetChunkPrivate *priv;

  g_return_if_fail (GB_IS_SOURCE_SNIPPET_CHUNK (chunk));
  g_return_if_fail (!context || GB_IS_SOURCE_SNIPPET_CONTEXT (context));

  priv = chunk->priv;

  if (context != chunk->priv->context)
    {
      if (priv->context_changed_handler)
        {
          g_signal_handler_disconnect (priv->context,
                                       priv->context_changed_handler);
          priv->context_changed_handler = 0;
        }

      g_clear_object (&chunk->priv->context);

      if (context)
        {
          priv->context = context ? g_object_ref (context) : NULL;
          priv->context_changed_handler =
            g_signal_connect_object (priv->context,
                                     "changed",
                                     G_CALLBACK (on_context_changed),
                                     chunk,
                                     0);
        }

      g_object_notify_by_pspec (G_OBJECT (chunk), gParamSpecs[PROP_CONTEXT]);
    }
}

const gchar *
gb_source_snippet_chunk_get_spec (GbSourceSnippetChunk *chunk)
{
  g_return_val_if_fail (GB_IS_SOURCE_SNIPPET_CHUNK (chunk), NULL);
  return chunk->priv->spec;
}

void
gb_source_snippet_chunk_set_spec (GbSourceSnippetChunk *chunk,
                                  const gchar          *spec)
{
  g_return_if_fail (GB_IS_SOURCE_SNIPPET_CHUNK (chunk));

  g_free (chunk->priv->spec);
  chunk->priv->spec = g_strdup (spec);
  g_object_notify_by_pspec (G_OBJECT (chunk), gParamSpecs[PROP_SPEC]);
}

gint
gb_source_snippet_chunk_get_tab_stop (GbSourceSnippetChunk *chunk)
{
  g_return_val_if_fail (GB_IS_SOURCE_SNIPPET_CHUNK (chunk), 0);
  return chunk->priv->tab_stop;
}

void
gb_source_snippet_chunk_set_tab_stop (GbSourceSnippetChunk *chunk,
                                      gint                  tab_stop)
{
  g_return_if_fail (GB_IS_SOURCE_SNIPPET_CHUNK (chunk));
  chunk->priv->tab_stop = tab_stop;
  g_object_notify_by_pspec (G_OBJECT (chunk), gParamSpecs[PROP_TAB_STOP]);
}

const gchar *
gb_source_snippet_chunk_get_text (GbSourceSnippetChunk *chunk)
{
  g_return_val_if_fail (GB_IS_SOURCE_SNIPPET_CHUNK (chunk), NULL);
  return chunk->priv->text ? chunk->priv->text : "";
}

void
gb_source_snippet_chunk_set_text (GbSourceSnippetChunk *chunk,
                                  const gchar          *text)
{
  g_return_if_fail (GB_IS_SOURCE_SNIPPET_CHUNK (chunk));

  g_free (chunk->priv->text);
  chunk->priv->text = g_strdup (text);
  g_object_notify_by_pspec (G_OBJECT (chunk), gParamSpecs[PROP_TEXT]);
}

gboolean
gb_source_snippet_chunk_get_text_set (GbSourceSnippetChunk *chunk)
{
  g_return_val_if_fail (GB_IS_SOURCE_SNIPPET_CHUNK (chunk), FALSE);
  return chunk->priv->text_set;
}

void
gb_source_snippet_chunk_set_text_set (GbSourceSnippetChunk *chunk,
                                      gboolean              text_set)
{
  g_return_if_fail (GB_IS_SOURCE_SNIPPET_CHUNK (chunk));
  chunk->priv->text_set = text_set;
  g_object_notify_by_pspec (G_OBJECT (chunk), gParamSpecs[PROP_TEXT_SET]);
}

static void
gb_source_snippet_chunk_finalize (GObject *object)
{
  GbSourceSnippetChunkPrivate *priv;

  priv = GB_SOURCE_SNIPPET_CHUNK (object)->priv;

  g_clear_pointer (&priv->spec, g_free);
  g_clear_pointer (&priv->text, g_free);
  g_clear_object (&priv->context);

  G_OBJECT_CLASS (gb_source_snippet_chunk_parent_class)->finalize (object);
}

static void
gb_source_snippet_chunk_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  GbSourceSnippetChunk *chunk = GB_SOURCE_SNIPPET_CHUNK (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, gb_source_snippet_chunk_get_context (chunk));
      break;

    case PROP_SPEC:
      g_value_set_string (value, gb_source_snippet_chunk_get_spec (chunk));
      break;

    case PROP_TAB_STOP:
      g_value_set_int (value, gb_source_snippet_chunk_get_tab_stop (chunk));
      break;

    case PROP_TEXT:
      g_value_set_string (value, gb_source_snippet_chunk_get_text (chunk));
      break;

    case PROP_TEXT_SET:
      g_value_set_boolean (value, gb_source_snippet_chunk_get_text_set (chunk));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_source_snippet_chunk_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  GbSourceSnippetChunk *chunk = GB_SOURCE_SNIPPET_CHUNK (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      gb_source_snippet_chunk_set_context (chunk, g_value_get_object (value));
      break;

    case PROP_TAB_STOP:
      gb_source_snippet_chunk_set_tab_stop (chunk, g_value_get_int (value));
      break;

    case PROP_SPEC:
      gb_source_snippet_chunk_set_spec (chunk, g_value_get_string (value));
      break;

    case PROP_TEXT:
      gb_source_snippet_chunk_set_text (chunk, g_value_get_string (value));
      break;

    case PROP_TEXT_SET:
      gb_source_snippet_chunk_set_text_set (chunk, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_source_snippet_chunk_class_init (GbSourceSnippetChunkClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = gb_source_snippet_chunk_finalize;
  object_class->get_property = gb_source_snippet_chunk_get_property;
  object_class->set_property = gb_source_snippet_chunk_set_property;
  g_type_class_add_private (object_class, sizeof (GbSourceSnippetChunkPrivate));

  gParamSpecs[PROP_CONTEXT] =
    g_param_spec_object ("context",
                         _("Context"),
                         _("The snippet context."),
                         GB_TYPE_SOURCE_SNIPPET_CONTEXT,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_CONTEXT,
                                   gParamSpecs[PROP_CONTEXT]);

  gParamSpecs[PROP_SPEC] =
    g_param_spec_string ("spec",
                         _("Spec"),
                         _("The specification to expand using the contxt."),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SPEC,
                                   gParamSpecs[PROP_SPEC]);

  gParamSpecs[PROP_TAB_STOP] =
    g_param_spec_int ("tab-stop",
                      _("Tab Stop"),
                      _("The tab stop for the chunk."),
                      -1,
                      G_MAXINT,
                      -1,
                      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_TAB_STOP,
                                   gParamSpecs[PROP_TAB_STOP]);

  gParamSpecs[PROP_TEXT] =
    g_param_spec_string ("text",
                         _("Text"),
                         _("The text for the chunk."),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_TEXT,
                                   gParamSpecs[PROP_TEXT]);

  gParamSpecs[PROP_TEXT_SET] =
    g_param_spec_boolean ("text-set",
                          _("Text Set"),
                          _("If the text property has been manually set."),
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_TEXT_SET,
                                   gParamSpecs[PROP_TEXT_SET]);
}

static void
gb_source_snippet_chunk_init (GbSourceSnippetChunk *chunk)
{
  chunk->priv =
    G_TYPE_INSTANCE_GET_PRIVATE (chunk,
                                 GB_TYPE_SOURCE_SNIPPET_CHUNK,
                                 GbSourceSnippetChunkPrivate);

  chunk->priv->tab_stop = -1;

  chunk->priv->spec = g_strdup ("");
}
