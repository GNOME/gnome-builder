/* gb-editor-file-mark.c
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

#include "gb-editor-file-mark.h"

struct _GbEditorFileMarkPrivate
{
  GFile *file;
  guint  line;
  guint  column;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbEditorFileMark, gb_editor_file_mark,
                            G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_COLUMN,
  PROP_FILE,
  PROP_LINE,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

GbEditorFileMark *
gb_editor_file_mark_new (GFile *file,
                         guint  line,
                         guint  column)
{
  return g_object_new (GB_TYPE_EDITOR_FILE_MARK,
                       "column", column,
                       "file", file,
                       "line", line,
                       NULL);
}

/**
 * gb_editor_file_mark_get_file:
 *
 * Fetches the file the mark is for.
 *
 * Returns: (transfer none): A #GFile.
 */
GFile *
gb_editor_file_mark_get_file (GbEditorFileMark *mark)
{
  g_return_val_if_fail (GB_IS_EDITOR_FILE_MARK (mark), NULL);

  return mark->priv->file;
}

guint
gb_editor_file_mark_get_line (GbEditorFileMark *mark)
{
  g_return_val_if_fail (GB_IS_EDITOR_FILE_MARK (mark), 0);

  return mark->priv->line;
}

guint
gb_editor_file_mark_get_column (GbEditorFileMark *mark)
{
  g_return_val_if_fail (GB_IS_EDITOR_FILE_MARK (mark), 0);

  return mark->priv->column;
}

void
gb_editor_file_mark_set_file (GbEditorFileMark *mark,
                              GFile            *file)
{
  g_return_if_fail (GB_IS_EDITOR_FILE_MARK (mark));
  g_return_if_fail (!file || G_IS_FILE (file));

  if (mark->priv->file != file)
    {
      g_clear_object (&mark->priv->file);
      mark->priv->file = file ? g_object_ref (file) : NULL;
      g_object_notify_by_pspec (G_OBJECT (mark), gParamSpecs [PROP_FILE]);
    }
}

void
gb_editor_file_mark_set_column (GbEditorFileMark *mark,
                                guint             column)
{
  g_return_if_fail (GB_IS_EDITOR_FILE_MARK (mark));

  if (mark->priv->column != column)
    {
      mark->priv->column = column;
      g_object_notify_by_pspec (G_OBJECT (mark), gParamSpecs [PROP_COLUMN]);
    }
}

void
gb_editor_file_mark_set_line (GbEditorFileMark *mark,
                              guint             line)
{
  g_return_if_fail (GB_IS_EDITOR_FILE_MARK (mark));

  if (mark->priv->line != line)
    {
      mark->priv->line = line;
      g_object_notify_by_pspec (G_OBJECT (mark), gParamSpecs [PROP_LINE]);
    }
}

static void
gb_editor_file_mark_finalize (GObject *object)
{
  GbEditorFileMarkPrivate *priv = GB_EDITOR_FILE_MARK (object)->priv;

  g_clear_object (&priv->file);

  G_OBJECT_CLASS (gb_editor_file_mark_parent_class)->finalize (object);
}

static void
gb_editor_file_mark_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GbEditorFileMark *self = GB_EDITOR_FILE_MARK (object);

  switch (prop_id)
    {
    case PROP_COLUMN:
      g_value_set_uint (value, gb_editor_file_mark_get_column (self));
      break;

    case PROP_LINE:
      g_value_set_uint (value, gb_editor_file_mark_get_line (self));
      break;

    case PROP_FILE:
      g_value_set_object (value, gb_editor_file_mark_get_file (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_editor_file_mark_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GbEditorFileMark *self = GB_EDITOR_FILE_MARK (object);

  switch (prop_id)
    {
    case PROP_COLUMN:
      gb_editor_file_mark_set_column (self, g_value_get_uint (value));
      break;

    case PROP_LINE:
      gb_editor_file_mark_set_line (self, g_value_get_uint (value));
      break;

    case PROP_FILE:
      gb_editor_file_mark_set_file (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_editor_file_mark_class_init (GbEditorFileMarkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gb_editor_file_mark_finalize;
  object_class->get_property = gb_editor_file_mark_get_property;
  object_class->set_property = gb_editor_file_mark_set_property;

  gParamSpecs [PROP_COLUMN] =
    g_param_spec_uint ("column",
                       _("Column"),
                       _("The column within the line."),
                       0,
                       G_MAXUINT,
                       0,
                       (G_PARAM_READWRITE |
                        G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_COLUMN,
                                   gParamSpecs [PROP_COLUMN]);

  gParamSpecs [PROP_FILE] =
    g_param_spec_object ("file",
                         _("File"),
                         _("The file for which to store the mark."),
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_FILE,
                                   gParamSpecs [PROP_FILE]);

  gParamSpecs [PROP_LINE] =
    g_param_spec_uint ("line",
                       _("Line"),
                       _("The line within the file."),
                       0,
                       G_MAXUINT,
                       0,
                       (G_PARAM_READWRITE |
                        G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_LINE,
                                   gParamSpecs [PROP_LINE]);
}

static void
gb_editor_file_mark_init (GbEditorFileMark *self)
{
  self->priv = gb_editor_file_mark_get_instance_private (self);
}
