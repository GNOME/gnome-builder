/* gb-editor-document.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#include "gb-editor-document.h"

struct _GbEditorDocumentPrivate
{
  GFile *file;
};

enum {
  PROP_0,
  PROP_FILE,
  LAST_PROP
};

enum {
  CURSOR_MOVED,
  LAST_SIGNAL
};

G_DEFINE_TYPE_WITH_PRIVATE (GbEditorDocument, gb_editor_document, GTK_SOURCE_TYPE_BUFFER)

static GParamSpec * gParamSpecs[LAST_PROP];
static guint gSignals[LAST_SIGNAL];

GbEditorDocument *
gb_editor_document_new (void)
{
  return g_object_new (GB_TYPE_EDITOR_DOCUMENT, NULL);
}

/**
 * gb_editor_document_get_file:
 * @document: A #GbEditorDocument.
 *
 * Returns the file backing the document.
 *
 * Returns: (transfer none): A #GFile.
 */
GFile *
gb_editor_document_get_file (GbEditorDocument *document)
{
  g_return_val_if_fail (GB_IS_EDITOR_DOCUMENT (document), NULL);

  return document->priv->file;
}

void
gb_editor_document_set_file (GbEditorDocument *document,
                             GFile            *file)
{
  GbEditorDocumentPrivate *priv;

  g_return_if_fail (GB_IS_EDITOR_DOCUMENT (document));
  g_return_if_fail (!file || G_IS_FILE (file));

  priv = document->priv;

  g_clear_object (&priv->file);

  if (file)
    priv->file = g_object_ref (file);

  g_object_notify_by_pspec (G_OBJECT (document), gParamSpecs[PROP_FILE]);
}

static void
gb_editor_document_mark_set (GtkTextBuffer     *buffer,
                             const GtkTextIter *iter,
                             GtkTextMark       *mark)
{
  if (GTK_TEXT_BUFFER_CLASS (gb_editor_document_parent_class)->mark_set)
    GTK_TEXT_BUFFER_CLASS (gb_editor_document_parent_class)->mark_set (buffer, iter, mark);

  if (mark == gtk_text_buffer_get_insert (buffer))
    g_signal_emit (buffer, gSignals[CURSOR_MOVED], 0);
}

static void
gb_editor_document_changed (GtkTextBuffer *buffer)
{
  g_signal_emit (buffer, gSignals[CURSOR_MOVED], 0);

  GTK_TEXT_BUFFER_CLASS (gb_editor_document_parent_class)->changed (buffer);
}

static void
gb_editor_document_finalize (GObject *object)
{
  GbEditorDocumentPrivate *priv;

  priv = GB_EDITOR_DOCUMENT (object)->priv;

  g_clear_object (&priv->file);

  G_OBJECT_CLASS (gb_editor_document_parent_class)->finalize (object);
}

static void
gb_editor_document_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GbEditorDocument *document = GB_EDITOR_DOCUMENT (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, gb_editor_document_get_file (document));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_editor_document_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GbEditorDocument *document = GB_EDITOR_DOCUMENT (object);

  switch (prop_id)
    {
    case PROP_FILE:
      gb_editor_document_set_file (document, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_editor_document_class_init (GbEditorDocumentClass *klass)
{
  GObjectClass *object_class;
  GtkTextBufferClass *text_buffer_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = gb_editor_document_finalize;
  object_class->get_property = gb_editor_document_get_property;
  object_class->set_property = gb_editor_document_set_property;

  text_buffer_class = GTK_TEXT_BUFFER_CLASS (klass);
  text_buffer_class->mark_set = gb_editor_document_mark_set;
  text_buffer_class->changed = gb_editor_document_changed;

  gParamSpecs[PROP_FILE] =
    g_param_spec_object ("file",
                         _ ("File"),
                         _ ("The file backing the document."),
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_FILE,
                                   gParamSpecs[PROP_FILE]);

  gSignals[CURSOR_MOVED] =
    g_signal_new ("cursor-moved",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GbEditorDocumentClass, cursor_moved),
                  NULL,
                  NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);
}

static void
gb_editor_document_init (GbEditorDocument *document)
{
  document->priv = gb_editor_document_get_instance_private (document);
}
