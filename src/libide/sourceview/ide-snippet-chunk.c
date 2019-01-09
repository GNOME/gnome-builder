/* ide-snippet-chunk.c
 *
 * Copyright 2013-2019 Christian Hergert <christian@hergert.me>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "ide-source-snippet"

#include "config.h"

#include <glib/gi18n.h>

#include "ide-snippet-chunk.h"
#include "ide-snippet-context.h"

/**
 * SECTION:ide-snippet-chunk
 * @title: IdeSnippetChunk
 * @short_description: An chunk of text within the source snippet
 *
 * The #IdeSnippetChunk represents a single chunk of text that
 * may or may not be an edit point within the snippet. Chunks that are
 * an edit point (also called a tab stop) have the
 * #IdeSnippetChunk:tab-stop property set.
 *
 * Since: 3.32
 */

struct _IdeSnippetChunk
{
  GObject            parent_instance;

  IdeSnippetContext *context;
  guint              context_changed_handler;
  gint               tab_stop;
  gchar             *spec;
  gchar             *text;
  guint              text_set : 1;
};

G_DEFINE_TYPE (IdeSnippetChunk, ide_snippet_chunk, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_CONTEXT,
  PROP_SPEC,
  PROP_TAB_STOP,
  PROP_TEXT,
  PROP_TEXT_SET,
  LAST_PROP
};

static GParamSpec *properties[LAST_PROP];

IdeSnippetChunk *
ide_snippet_chunk_new (void)
{
  return g_object_new (IDE_TYPE_SNIPPET_CHUNK, NULL);
}

/**
 * ide_snippet_chunk_copy:
 *
 * Copies the source snippet.
 *
 * Returns: (transfer full): An #IdeSnippetChunk.
 *
 * Since: 3.32
 */
IdeSnippetChunk *
ide_snippet_chunk_copy (IdeSnippetChunk *chunk)
{
  IdeSnippetChunk *ret;

  g_return_val_if_fail (IDE_IS_SNIPPET_CHUNK (chunk), NULL);

  ret = g_object_new (IDE_TYPE_SNIPPET_CHUNK,
                      "spec", chunk->spec,
                      "tab-stop", chunk->tab_stop,
                      NULL);

  return ret;
}

static void
on_context_changed (IdeSnippetContext *context,
                    IdeSnippetChunk   *chunk)
{
  gchar *text;

  g_assert (IDE_IS_SNIPPET_CHUNK (chunk));
  g_assert (IDE_IS_SNIPPET_CONTEXT (context));

  if (!chunk->text_set)
    {
      text = ide_snippet_context_expand (context, chunk->spec);
      ide_snippet_chunk_set_text (chunk, text);
      g_free (text);
    }
}

/**
 * ide_snippet_chunk_get_context:
 *
 * Gets the context for the snippet insertion.
 *
 * Returns: (transfer none): An #IdeSnippetContext.
 *
 * Since: 3.32
 */
IdeSnippetContext *
ide_snippet_chunk_get_context (IdeSnippetChunk *chunk)
{
  g_return_val_if_fail (IDE_IS_SNIPPET_CHUNK (chunk), NULL);

  return chunk->context;
}

void
ide_snippet_chunk_set_context (IdeSnippetChunk   *chunk,
                               IdeSnippetContext *context)
{
  g_return_if_fail (IDE_IS_SNIPPET_CHUNK (chunk));
  g_return_if_fail (!context || IDE_IS_SNIPPET_CONTEXT (context));

  if (context != chunk->context)
    {
      if (chunk->context_changed_handler)
        {
          g_signal_handler_disconnect (chunk->context,
                                       chunk->context_changed_handler);
          chunk->context_changed_handler = 0;
        }

      g_clear_object (&chunk->context);

      if (context != NULL)
        {
          chunk->context = g_object_ref (context);
          chunk->context_changed_handler =
            g_signal_connect_object (chunk->context,
                                     "changed",
                                     G_CALLBACK (on_context_changed),
                                     chunk,
                                     0);
        }

      g_object_notify_by_pspec (G_OBJECT (chunk), properties[PROP_CONTEXT]);
    }
}

const gchar *
ide_snippet_chunk_get_spec (IdeSnippetChunk *chunk)
{
  g_return_val_if_fail (IDE_IS_SNIPPET_CHUNK (chunk), NULL);
  return chunk->spec;
}

void
ide_snippet_chunk_set_spec (IdeSnippetChunk *chunk,
                            const gchar     *spec)
{
  g_return_if_fail (IDE_IS_SNIPPET_CHUNK (chunk));

  g_free (chunk->spec);
  chunk->spec = g_strdup (spec);
  g_object_notify_by_pspec (G_OBJECT (chunk), properties[PROP_SPEC]);
}

gint
ide_snippet_chunk_get_tab_stop (IdeSnippetChunk *chunk)
{
  g_return_val_if_fail (IDE_IS_SNIPPET_CHUNK (chunk), 0);
  return chunk->tab_stop;
}

void
ide_snippet_chunk_set_tab_stop (IdeSnippetChunk *chunk,
                                gint             tab_stop)
{
  g_return_if_fail (IDE_IS_SNIPPET_CHUNK (chunk));
  chunk->tab_stop = tab_stop;
  g_object_notify_by_pspec (G_OBJECT (chunk), properties[PROP_TAB_STOP]);
}

const gchar *
ide_snippet_chunk_get_text (IdeSnippetChunk *chunk)
{
  g_return_val_if_fail (IDE_IS_SNIPPET_CHUNK (chunk), NULL);
  return chunk->text ? chunk->text : "";
}

void
ide_snippet_chunk_set_text (IdeSnippetChunk *chunk,
                            const gchar     *text)
{
  g_return_if_fail (IDE_IS_SNIPPET_CHUNK (chunk));

  if (chunk->text != text)
    {
      g_free (chunk->text);
      chunk->text = g_strdup (text);
      g_object_notify_by_pspec (G_OBJECT (chunk), properties[PROP_TEXT]);
    }
}

gboolean
ide_snippet_chunk_get_text_set (IdeSnippetChunk *chunk)
{
  g_return_val_if_fail (IDE_IS_SNIPPET_CHUNK (chunk), FALSE);

  return chunk->text_set;
}

void
ide_snippet_chunk_set_text_set (IdeSnippetChunk *chunk,
                                gboolean         text_set)
{
  g_return_if_fail (IDE_IS_SNIPPET_CHUNK (chunk));

  text_set = !!text_set;

  if (chunk->text_set != text_set)
    {
      chunk->text_set = text_set;
      g_object_notify_by_pspec (G_OBJECT (chunk), properties[PROP_TEXT_SET]);
    }
}

static void
ide_snippet_chunk_finalize (GObject *object)
{
  IdeSnippetChunk *chunk = (IdeSnippetChunk *)object;

  g_clear_pointer (&chunk->spec, g_free);
  g_clear_pointer (&chunk->text, g_free);
  g_clear_object (&chunk->context);

  G_OBJECT_CLASS (ide_snippet_chunk_parent_class)->finalize (object);
}

static void
ide_snippet_chunk_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  IdeSnippetChunk *chunk = IDE_SNIPPET_CHUNK (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, ide_snippet_chunk_get_context (chunk));
      break;

    case PROP_SPEC:
      g_value_set_string (value, ide_snippet_chunk_get_spec (chunk));
      break;

    case PROP_TAB_STOP:
      g_value_set_int (value, ide_snippet_chunk_get_tab_stop (chunk));
      break;

    case PROP_TEXT:
      g_value_set_string (value, ide_snippet_chunk_get_text (chunk));
      break;

    case PROP_TEXT_SET:
      g_value_set_boolean (value, ide_snippet_chunk_get_text_set (chunk));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_snippet_chunk_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  IdeSnippetChunk *chunk = IDE_SNIPPET_CHUNK (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      ide_snippet_chunk_set_context (chunk, g_value_get_object (value));
      break;

    case PROP_TAB_STOP:
      ide_snippet_chunk_set_tab_stop (chunk, g_value_get_int (value));
      break;

    case PROP_SPEC:
      ide_snippet_chunk_set_spec (chunk, g_value_get_string (value));
      break;

    case PROP_TEXT:
      ide_snippet_chunk_set_text (chunk, g_value_get_string (value));
      break;

    case PROP_TEXT_SET:
      ide_snippet_chunk_set_text_set (chunk, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_snippet_chunk_class_init (IdeSnippetChunkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_snippet_chunk_finalize;
  object_class->get_property = ide_snippet_chunk_get_property;
  object_class->set_property = ide_snippet_chunk_set_property;

  properties[PROP_CONTEXT] =
    g_param_spec_object ("context",
                         "Context",
                         "The snippet context.",
                         IDE_TYPE_SNIPPET_CONTEXT,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties[PROP_SPEC] =
    g_param_spec_string ("spec",
                         "Spec",
                         "The specification to expand using the context.",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties[PROP_TAB_STOP] =
    g_param_spec_int ("tab-stop",
                      "Tab Stop",
                      "The tab stop for the chunk.",
                      -1,
                      G_MAXINT,
                      -1,
                      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties[PROP_TEXT] =
    g_param_spec_string ("text",
                         "Text",
                         "The text for the chunk.",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties[PROP_TEXT_SET] =
    g_param_spec_boolean ("text-set",
                          "Text Set",
                          "If the text property has been manually set.",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
ide_snippet_chunk_init (IdeSnippetChunk *chunk)
{
  chunk->tab_stop = -1;
  chunk->spec = g_strdup ("");
}
