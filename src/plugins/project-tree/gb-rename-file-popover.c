/* gb-rename-file-popover.c
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

#include "gb-rename-file-popover.h"

struct _GbRenameFilePopover
{
  GtkPopover    parent_instance;

  GCancellable *cancellable;
  GFile        *file;

  GtkEntry     *entry;
  GtkButton    *button;
  GtkLabel     *label;
  GtkLabel     *message;

  guint         is_directory : 1;
};

enum {
  PROP_0,
  PROP_FILE,
  PROP_IS_DIRECTORY,
  LAST_PROP
};

enum {
  RENAME_FILE,
  LAST_SIGNAL
};

G_DEFINE_TYPE (GbRenameFilePopover, gb_rename_file_popover, GTK_TYPE_POPOVER)

static GParamSpec *properties [LAST_PROP];
static guint signals [LAST_SIGNAL];

GFile *
gb_rename_file_popover_get_file (GbRenameFilePopover *self)
{
  g_return_val_if_fail (GB_IS_RENAME_FILE_POPOVER (self), NULL);

  return self->file;
}

static void
gb_rename_file_popover_set_file (GbRenameFilePopover *self,
                                 GFile               *file)
{
  g_return_if_fail (GB_IS_RENAME_FILE_POPOVER (self));
  g_return_if_fail (G_IS_FILE (file));

  if (g_set_object (&self->file, file))
    {
      if (file != NULL)
        {
          g_autofree gchar *name = NULL;
          g_autofree gchar *label = NULL;

          name = g_file_get_basename (file);
          label = g_strdup_printf (_("Rename %s"), name);

          gtk_label_set_label (self->label, label);
          gtk_entry_set_text (self->entry, name);
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FILE]);
    }
}

static void
gb_rename_file_popover_set_is_directory (GbRenameFilePopover *self,
                                         gboolean             is_directory)
{
  g_return_if_fail (GB_IS_RENAME_FILE_POPOVER (self));

  is_directory = !!is_directory;

  if (is_directory != self->is_directory)
    {
      self->is_directory = is_directory;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_IS_DIRECTORY]);
    }
}

static void
gb_rename_file_popover__file_query_info (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  GFile *file = (GFile *)object;
  g_autoptr(GFileInfo) file_info = NULL;
  g_autoptr(GbRenameFilePopover) self = user_data;
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
gb_rename_file_popover__entry_changed (GbRenameFilePopover *self,
                                       GtkEntry            *entry)
{
  g_autoptr(GFile) parent = NULL;
  g_autoptr(GFile) file = NULL;
  const gchar *text;

  g_assert (GB_IS_RENAME_FILE_POPOVER (self));
  g_assert (GTK_IS_ENTRY (entry));
  g_assert (self->file != NULL);
  g_assert (G_IS_FILE (self->file));

  gtk_widget_set_sensitive (GTK_WIDGET (self->button), FALSE);
  gtk_label_set_label (self->message, NULL);

  text = gtk_entry_get_text (entry);
  if (ide_str_empty0 (text))
    return;

  if (self->cancellable)
    {
      g_cancellable_cancel (self->cancellable);
      g_clear_object (&self->cancellable);
    }

  self->cancellable = g_cancellable_new ();

  parent = g_file_get_parent (self->file);
  file = g_file_get_child (parent, text);

  g_file_query_info_async (file,
                           G_FILE_ATTRIBUTE_STANDARD_TYPE,
                           G_FILE_QUERY_INFO_NONE,
                           G_PRIORITY_DEFAULT,
                           self->cancellable,
                           gb_rename_file_popover__file_query_info,
                           g_object_ref (self));
}

static void
gb_rename_file_popover__entry_activate (GbRenameFilePopover *self,
                                        GtkEntry            *entry)
{
  g_assert (GB_IS_RENAME_FILE_POPOVER (self));
  g_assert (GTK_IS_ENTRY (entry));

  if (gtk_widget_get_sensitive (GTK_WIDGET (self->button)))
    gtk_widget_activate (GTK_WIDGET (self->button));
}

static void
gb_rename_file_popover__entry_focus_in_event (GbRenameFilePopover *self,
                                              GdkEvent            *event,
                                              GtkEntry            *entry)
{
  const gchar *name;
  const gchar *tmp;

  g_assert (GB_IS_RENAME_FILE_POPOVER (self));
  g_assert (GTK_IS_ENTRY (entry));

  name = gtk_entry_get_text (entry);

  if (NULL != (tmp = strrchr (name, '.')))
    gtk_editable_select_region (GTK_EDITABLE (entry), 0, tmp - name);
}

static void
gb_rename_file_popover__button_clicked (GbRenameFilePopover *self,
                                        GtkButton           *button)
{
  g_autoptr(GFile) file = NULL;
  g_autoptr(GFile) parent = NULL;
  const gchar *path;

  g_assert (GB_IS_RENAME_FILE_POPOVER (self));
  g_assert (GTK_IS_BUTTON (button));
  g_assert (self->file != NULL);
  g_assert (G_IS_FILE (self->file));

  path = gtk_entry_get_text (self->entry);
  if (ide_str_empty0 (path))
    return;

  parent = g_file_get_parent (self->file);
  file = g_file_get_child (parent, path);

  /* only activate once */
  gtk_widget_set_sensitive (GTK_WIDGET (self->button), FALSE);

  g_signal_emit (self, signals [RENAME_FILE], 0, self->file, file);
}

static void
gb_rename_file_popover_finalize (GObject *object)
{
  GbRenameFilePopover *self = (GbRenameFilePopover *)object;

  if (self->cancellable != NULL)
    {
      if (!g_cancellable_is_cancelled (self->cancellable))
        g_cancellable_cancel (self->cancellable);
      g_clear_object (&self->cancellable);
    }

  g_clear_object (&self->file);

  G_OBJECT_CLASS (gb_rename_file_popover_parent_class)->finalize (object);
}

static void
gb_rename_file_popover_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GbRenameFilePopover *self = GB_RENAME_FILE_POPOVER (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, self->file);
      break;

    case PROP_IS_DIRECTORY:
      g_value_set_boolean (value, self->is_directory);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_rename_file_popover_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GbRenameFilePopover *self = GB_RENAME_FILE_POPOVER (object);

  switch (prop_id)
    {
    case PROP_FILE:
      gb_rename_file_popover_set_file (self, g_value_get_object (value));
      break;

    case PROP_IS_DIRECTORY:
      gb_rename_file_popover_set_is_directory (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_rename_file_popover_class_init (GbRenameFilePopoverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gb_rename_file_popover_finalize;
  object_class->get_property = gb_rename_file_popover_get_property;
  object_class->set_property = gb_rename_file_popover_set_property;

  properties [PROP_FILE] =
    g_param_spec_object ("file",
                         "File",
                         "File",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_IS_DIRECTORY] =
    g_param_spec_boolean ("is-directory",
                          "Is Directory",
                          "Is Directory",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  signals [RENAME_FILE] =
    g_signal_new ("rename-file",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  2,
                  G_TYPE_FILE,
                  G_TYPE_FILE);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/plugins/project-tree-plugin/gb-rename-file-popover.ui");
  gtk_widget_class_bind_template_child (widget_class, GbRenameFilePopover, button);
  gtk_widget_class_bind_template_child (widget_class, GbRenameFilePopover, entry);
  gtk_widget_class_bind_template_child (widget_class, GbRenameFilePopover, label);
  gtk_widget_class_bind_template_child (widget_class, GbRenameFilePopover, message);
}

static void
gb_rename_file_popover_init (GbRenameFilePopover *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (self->entry,
                           "changed",
                           G_CALLBACK (gb_rename_file_popover__entry_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->entry,
                           "activate",
                           G_CALLBACK (gb_rename_file_popover__entry_activate),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->button,
                           "clicked",
                           G_CALLBACK (gb_rename_file_popover__button_clicked),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->entry,
                           "focus-in-event",
                           G_CALLBACK (gb_rename_file_popover__entry_focus_in_event),
                           self,
                           G_CONNECT_SWAPPED | G_CONNECT_AFTER);
}
