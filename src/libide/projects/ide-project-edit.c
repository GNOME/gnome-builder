/* ide-project-edit.c
 *
 * Copyright 2016 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-project-edit"

#include "config.h"

#include <dazzle.h>

#include "projects/ide-project-edit.h"
#include "projects/ide-project-edit-private.h"

typedef struct
{
  IdeSourceRange *range;
  gchar          *replacement;

  /* No references, cleared in apply */
  GtkTextMark    *begin_mark;
  GtkTextMark    *end_mark;
} IdeProjectEditPrivate;

enum {
  PROP_0,
  PROP_RANGE,
  PROP_REPLACEMENT,
  N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (IdeProjectEdit, ide_project_edit, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
ide_project_edit_finalize (GObject *object)
{
  IdeProjectEdit *self = (IdeProjectEdit *)object;
  IdeProjectEditPrivate *priv = ide_project_edit_get_instance_private (self);

  dzl_clear_pointer (&priv->range, ide_source_range_unref);
  dzl_clear_pointer (&priv->replacement, g_free);

  G_OBJECT_CLASS (ide_project_edit_parent_class)->finalize (object);
}

static void
ide_project_edit_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  IdeProjectEdit *self = (IdeProjectEdit *)object;

  switch (prop_id)
    {
    case PROP_RANGE:
      g_value_set_boxed (value, ide_project_edit_get_range (self));
      break;

    case PROP_REPLACEMENT:
      g_value_set_string (value, ide_project_edit_get_replacement (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_project_edit_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  IdeProjectEdit *self = (IdeProjectEdit *)object;

  switch (prop_id)
    {
    case PROP_RANGE:
      ide_project_edit_set_range (self, g_value_get_boxed (value));
      break;

    case PROP_REPLACEMENT:
      ide_project_edit_set_replacement (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_project_edit_class_init (IdeProjectEditClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_project_edit_finalize;
  object_class->get_property = ide_project_edit_get_property;
  object_class->set_property = ide_project_edit_set_property;

  properties [PROP_RANGE] =
    g_param_spec_boxed ("range",
                        "Range",
                        "The range of the source to replace",
                        IDE_TYPE_SOURCE_RANGE,
                        (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_REPLACEMENT] =
    g_param_spec_string ("replacement",
                         "Replacement",
                         "The replacement text to be applied.",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_project_edit_init (IdeProjectEdit *self)
{
}

IdeProjectEdit *
ide_project_edit_new (void)
{
  return g_object_new (IDE_TYPE_PROJECT_EDIT, NULL);
}

/**
 * ide_project_edit_get_range:
 *
 * Returns the range for the edit.
 *
 * Returns: (nullable) (transfer none): An #IdeSourceRange
 */
IdeSourceRange *
ide_project_edit_get_range (IdeProjectEdit *self)
{
  IdeProjectEditPrivate *priv = ide_project_edit_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_PROJECT_EDIT (self), NULL);

  return priv->range;
}

void
ide_project_edit_set_range (IdeProjectEdit *self,
                            IdeSourceRange *range)
{
  IdeProjectEditPrivate *priv = ide_project_edit_get_instance_private (self);

  g_return_if_fail (IDE_IS_PROJECT_EDIT (self));
  g_return_if_fail (range != NULL);

  if (priv->range != range)
    {
      dzl_clear_pointer (&priv->range, ide_source_range_unref);
      priv->range = ide_source_range_ref (range);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_RANGE]);
    }
}

const gchar *
ide_project_edit_get_replacement (IdeProjectEdit *self)
{
  IdeProjectEditPrivate *priv = ide_project_edit_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_PROJECT_EDIT (self), NULL);

  return priv->replacement;
}

void
ide_project_edit_set_replacement (IdeProjectEdit *self,
                                  const gchar    *replacement)
{
  IdeProjectEditPrivate *priv = ide_project_edit_get_instance_private (self);

  g_return_if_fail (IDE_IS_PROJECT_EDIT (self));

  if (g_strcmp0 (priv->replacement, replacement) != 0)
    {
      g_free (priv->replacement);
      priv->replacement = g_strdup (replacement);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_REPLACEMENT]);
    }
}

void
_ide_project_edit_prepare (IdeProjectEdit *self,
                           IdeBuffer      *buffer)
{
  IdeProjectEditPrivate *priv = ide_project_edit_get_instance_private (self);
  IdeSourceLocation *begin;
  IdeSourceLocation *end;
  GtkTextIter begin_iter;
  GtkTextIter end_iter;

  g_assert (IDE_IS_PROJECT_EDIT (self));
  g_assert (IDE_IS_BUFFER (buffer));

  begin = ide_source_range_get_begin (priv->range);
  end = ide_source_range_get_end (priv->range);

  ide_buffer_get_iter_at_source_location (buffer, &begin_iter, begin);
  ide_buffer_get_iter_at_source_location (buffer, &end_iter, end);

  priv->begin_mark = gtk_text_buffer_create_mark (GTK_TEXT_BUFFER (buffer),
                                                  NULL,
                                                  &begin_iter,
                                                  TRUE);

  priv->end_mark = gtk_text_buffer_create_mark (GTK_TEXT_BUFFER (buffer),
                                                NULL,
                                                &end_iter,
                                                FALSE);
}

void
_ide_project_edit_apply (IdeProjectEdit *self,
                         IdeBuffer      *buffer)
{
  IdeProjectEditPrivate *priv = ide_project_edit_get_instance_private (self);
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (IDE_IS_PROJECT_EDIT (self));
  g_assert (IDE_IS_BUFFER (buffer));

  gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer), &begin, priv->begin_mark);
  gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer), &end, priv->end_mark);
  gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &begin, &end);
  gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &begin, priv->replacement, -1);
  gtk_text_buffer_delete_mark (GTK_TEXT_BUFFER (buffer), priv->begin_mark);
  gtk_text_buffer_delete_mark (GTK_TEXT_BUFFER (buffer), priv->end_mark);
}
