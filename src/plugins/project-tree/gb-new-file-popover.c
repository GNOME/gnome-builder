/* gb-new-file-popover.c
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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
#include <ide.h>

#include "gb-new-file-popover.h"

struct _GbNewFilePopover
{
  GtkPopover    parent_instance;

  GFileType     file_type;
  GFile        *directory;
  GCancellable *cancellable;

  GtkButton    *button;
  GtkEntry     *entry;
  GtkLabel     *message;
  GtkLabel     *title;
};

G_DEFINE_TYPE (GbNewFilePopover, gb_new_file_popover, GTK_TYPE_POPOVER)

enum {
  PROP_0,
  PROP_DIRECTORY,
  PROP_FILE_TYPE,
  LAST_PROP
};

enum {
  CREATE_FILE,
  LAST_SIGNAL
};

static GParamSpec *properties [LAST_PROP];
static guint       signals [LAST_SIGNAL];

static void
gb_new_file_popover__button_clicked (GbNewFilePopover *self,
                                     GtkButton        *button)
{
  g_autoptr(GFile) file = NULL;
  const gchar *path;

  g_assert (GB_IS_NEW_FILE_POPOVER (self));
  g_assert (GTK_IS_BUTTON (button));

  if (self->directory == NULL)
    return;

  path = gtk_entry_get_text (self->entry);
  if (ide_str_empty0 (path))
    return;

  file = g_file_get_child (self->directory, path);

  g_signal_emit (self, signals [CREATE_FILE], 0, file, self->file_type);
}

static void
gb_new_file_popover__entry_activate (GbNewFilePopover *self,
                                     GtkEntry         *entry)
{
  g_assert (GB_IS_NEW_FILE_POPOVER (self));
  g_assert (GTK_IS_ENTRY (entry));

  if (gtk_widget_get_sensitive (GTK_WIDGET (self->button)))
    gtk_widget_activate (GTK_WIDGET (self->button));
}

static void
gb_new_file_popover__query_info_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  GFile *file = (GFile *)object;
  g_autoptr(GFileInfo) file_info = NULL;
  g_autoptr(GbNewFilePopover) self = user_data;
  g_autoptr(GError) error = NULL;
  GFileType file_type;

  file_info = g_file_query_info_finish (file, result, &error);

  if (file_info == NULL &&
      g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    return;

  if ((file_info == NULL) &&
      g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
    {
      gtk_label_set_label (self->message, NULL);
      gtk_widget_set_sensitive (GTK_WIDGET (self->button), TRUE);
      return;
    }

  if (file_info == NULL)
    {
      gtk_label_set_label (self->message, error->message);
      return;
    }

  file_type = g_file_info_get_file_type (file_info);

  if (file_type == G_FILE_TYPE_DIRECTORY)
    gtk_label_set_label (self->message,
                         _("A folder with that name already exists."));
  else
    gtk_label_set_label (self->message,
                         _("A file with that name already exists."));

  gtk_widget_set_sensitive (GTK_WIDGET (self->button), FALSE);
}

static void
gb_new_file_popover_check_exists (GbNewFilePopover *self,
                                  GFile            *directory,
                                  const gchar      *path)
{
  g_autoptr(GFile) child = NULL;

  g_assert (GB_IS_NEW_FILE_POPOVER (self));
  g_assert (!directory || G_IS_FILE (directory));

  if (self->cancellable != NULL)
    {
      if (!g_cancellable_is_cancelled (self->cancellable))
        g_cancellable_cancel (self->cancellable);
      g_clear_object (&self->cancellable);
    }

  gtk_label_set_label (self->message, NULL);
  gtk_widget_set_sensitive (GTK_WIDGET (self->button), FALSE);

  if (directory == NULL)
    return;

  if (ide_str_empty0 (path))
    return;

  child = g_file_get_child (directory, path);

  self->cancellable = g_cancellable_new ();

  g_file_query_info_async (child,
                           G_FILE_ATTRIBUTE_STANDARD_TYPE,
                           G_FILE_QUERY_INFO_NONE,
                           G_PRIORITY_DEFAULT,
                           self->cancellable,
                           gb_new_file_popover__query_info_cb,
                           g_object_ref (self));

}

static void
gb_new_file_popover__entry_changed (GbNewFilePopover *self,
                                    GtkEntry         *entry)
{
  const gchar *text;

  g_assert (GB_IS_NEW_FILE_POPOVER (self));
  g_assert (GTK_IS_ENTRY (entry));

  text = gtk_entry_get_text (entry);

  gtk_widget_set_sensitive (GTK_WIDGET (self->button), !ide_str_empty0 (text));

  gb_new_file_popover_check_exists (self, self->directory, text);
}

static void
gb_new_file_popover_finalize (GObject *object)
{
  GbNewFilePopover *self = (GbNewFilePopover *)object;

  if (self->cancellable && !g_cancellable_is_cancelled (self->cancellable))
    g_cancellable_cancel (self->cancellable);

  g_clear_object (&self->cancellable);
  g_clear_object (&self->directory);

  G_OBJECT_CLASS (gb_new_file_popover_parent_class)->finalize (object);
}

static void
gb_new_file_popover_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GbNewFilePopover *self = GB_NEW_FILE_POPOVER(object);

  switch (prop_id)
    {
    case PROP_DIRECTORY:
      g_value_set_object (value, gb_new_file_popover_get_directory (self));
      break;

    case PROP_FILE_TYPE:
      g_value_set_enum (value, gb_new_file_popover_get_file_type (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

/**
 * gb_new_file_popover_set_property:
 * @object: (in): a #GObject.
 * @prop_id: (in): The property identifier.
 * @value: (in): The given property.
 * @pspec: (in): a #ParamSpec.
 *
 * Set a given #GObject property.
 */
static void
gb_new_file_popover_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GbNewFilePopover *self = GB_NEW_FILE_POPOVER(object);

  switch (prop_id)
    {
    case PROP_DIRECTORY:
      gb_new_file_popover_set_directory (self, g_value_get_object (value));
      break;

    case PROP_FILE_TYPE:
      gb_new_file_popover_set_file_type (self, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
gb_new_file_popover_class_init (GbNewFilePopoverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gb_new_file_popover_finalize;
  object_class->get_property = gb_new_file_popover_get_property;
  object_class->set_property = gb_new_file_popover_set_property;

  properties [PROP_DIRECTORY] =
    g_param_spec_object ("directory",
                         "Directory",
                         "Directory",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_FILE_TYPE] =
    g_param_spec_enum ("file-type",
                       "File Type",
                       "The file type to create.",
                       G_TYPE_FILE_TYPE,
                       G_FILE_TYPE_REGULAR,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  signals [CREATE_FILE] =
    g_signal_new ("create-file",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  2,
                  G_TYPE_FILE,
                  G_TYPE_FILE_TYPE);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/plugins/project-tree-plugin/gb-new-file-popover.ui");
  gtk_widget_class_bind_template_child (widget_class, GbNewFilePopover, button);
  gtk_widget_class_bind_template_child (widget_class, GbNewFilePopover, entry);
  gtk_widget_class_bind_template_child (widget_class, GbNewFilePopover, message);
  gtk_widget_class_bind_template_child (widget_class, GbNewFilePopover, title);
}

static void
gb_new_file_popover_init (GbNewFilePopover *self)
{
  self->file_type = G_FILE_TYPE_REGULAR;

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (self->entry,
                           "activate",
                           G_CALLBACK (gb_new_file_popover__entry_activate),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->entry,
                           "changed",
                           G_CALLBACK (gb_new_file_popover__entry_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->button,
                           "clicked",
                           G_CALLBACK (gb_new_file_popover__button_clicked),
                           self,
                           G_CONNECT_SWAPPED);
}

GFileType
gb_new_file_popover_get_file_type (GbNewFilePopover *self)
{
  g_return_val_if_fail (GB_IS_NEW_FILE_POPOVER (self), 0);

  return self->file_type;
}

void
gb_new_file_popover_set_file_type (GbNewFilePopover *self,
                                   GFileType         file_type)
{
  g_return_if_fail (GB_IS_NEW_FILE_POPOVER (self));
  g_return_if_fail ((file_type == G_FILE_TYPE_REGULAR) ||
                    (file_type == G_FILE_TYPE_DIRECTORY));

  if (file_type != self->file_type)
    {
      self->file_type = file_type;

      if (file_type == G_FILE_TYPE_REGULAR)
        gtk_label_set_label (self->title, _("File Name"));
      else
        gtk_label_set_label (self->title, _("Folder Name"));

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FILE_TYPE]);
    }
}

void
gb_new_file_popover_set_directory (GbNewFilePopover *self,
                                   GFile            *directory)
{
  g_return_if_fail (GB_IS_NEW_FILE_POPOVER (self));
  g_return_if_fail (G_IS_FILE (directory));

  if (g_set_object (&self->directory, directory))
    {
      const gchar *path;

      path = gtk_entry_get_text (self->entry);
      gb_new_file_popover_check_exists (self, directory, path);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DIRECTORY]);
    }
}

/**
 * gb_new_file_popover_get_directory:
 *
 * Returns: (transfer none) (nullable): a #GFile or %NULL.
 */
GFile *
gb_new_file_popover_get_directory (GbNewFilePopover *self)
{
  g_return_val_if_fail (GB_IS_NEW_FILE_POPOVER (self), NULL);

  return self->directory;
}
