/* gbp-new-file-popover.c
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "gbp-new-file-popover"

#include <glib/gi18n.h>
#include <libide-gui.h>
#include <libide-threading.h>

#include "gbp-new-file-popover.h"

struct _GbpNewFilePopover
{
  GtkPopover    parent_instance;

  GFileType     file_type;
  GFile        *directory;
  IdeTask      *task;

  GtkButton    *button;
  GtkEntry     *entry;
  GtkLabel     *message;
  GtkLabel     *title;
};

G_DEFINE_FINAL_TYPE (GbpNewFilePopover, gbp_new_file_popover, GTK_TYPE_POPOVER)

enum {
  PROP_0,
  PROP_DIRECTORY,
  PROP_FILE_TYPE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
gbp_new_file_popover_button_clicked (GbpNewFilePopover *self,
                                     GtkButton        *button)
{
  g_autoptr(GFile) file = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autofree char *stripped = NULL;
  const gchar *path;

  g_assert (GBP_IS_NEW_FILE_POPOVER (self));
  g_assert (GTK_IS_BUTTON (button));

  if (self->directory == NULL)
    return;

  path = gtk_editable_get_text (GTK_EDITABLE (self->entry));
  if (ide_str_empty0 (path))
    return;

  stripped = g_strstrip (g_strdup (path));

  file = g_file_get_child (self->directory, stripped);

  if ((task = g_steal_pointer (&self->task)))
    ide_task_return_pointer (task, g_steal_pointer (&file), g_object_unref);
}

static void
gbp_new_file_popover_entry_activate (GbpNewFilePopover *self,
                                     GtkEntry         *entry)
{
  g_assert (GBP_IS_NEW_FILE_POPOVER (self));
  g_assert (GTK_IS_ENTRY (entry));

  if (gtk_widget_get_sensitive (GTK_WIDGET (self->button)))
    gtk_widget_activate (GTK_WIDGET (self->button));
}

static void
gbp_new_file_popover_query_info_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  GFile *file = (GFile *)object;
  g_autoptr(GFileInfo) file_info = NULL;
  g_autoptr(GbpNewFilePopover) self = user_data;
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
gbp_new_file_popover_check_exists (GbpNewFilePopover *self,
                                   GFile             *directory,
                                   const gchar       *path)
{
  g_autoptr(GFile) child = NULL;
  GCancellable *cancellable = NULL;

  g_assert (GBP_IS_NEW_FILE_POPOVER (self));
  g_assert (!directory || G_IS_FILE (directory));

  gtk_label_set_label (self->message, NULL);
  gtk_widget_set_sensitive (GTK_WIDGET (self->button), FALSE);

  if (directory == NULL)
    return;

  if (ide_str_empty0 (path))
    return;

  child = g_file_get_child (directory, path);

  if (self->task)
    cancellable = ide_task_get_cancellable (self->task);

  g_file_query_info_async (child,
                           G_FILE_ATTRIBUTE_STANDARD_TYPE,
                           G_FILE_QUERY_INFO_NONE,
                           G_PRIORITY_DEFAULT,
                           cancellable,
                           gbp_new_file_popover_query_info_cb,
                           g_object_ref (self));

}

static void
gbp_new_file_popover_entry_changed (GbpNewFilePopover *self,
                                    GtkEntry          *entry)
{
  g_autofree gchar *stripped = NULL;

  g_assert (GBP_IS_NEW_FILE_POPOVER (self));
  g_assert (GTK_IS_ENTRY (entry));

  /* make sure to strip so that warnings (eg. "file already exists") are
   * consistents with the final behavior (creating the file). */
  stripped = g_strstrip(g_strdup (gtk_editable_get_text (GTK_EDITABLE (entry))));

  gtk_widget_set_sensitive (GTK_WIDGET (self->button), !ide_str_empty0 (stripped));

  gbp_new_file_popover_check_exists (self, self->directory, stripped);
}

static void
gbp_new_file_popover_closed (GtkPopover *popover)
{
  GbpNewFilePopover *self = (GbpNewFilePopover *)popover;
  g_autoptr(IdeTask) task = NULL;

  g_assert (GBP_IS_NEW_FILE_POPOVER (self));

  if ((task = g_steal_pointer (&self->task)))
    ide_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_CANCELLED,
                               "The popover was closed");
}

static void
gbp_new_file_popover_finalize (GObject *object)
{
  GbpNewFilePopover *self = (GbpNewFilePopover *)object;

  g_assert (self->task == NULL);

  g_clear_object (&self->directory);

  G_OBJECT_CLASS (gbp_new_file_popover_parent_class)->finalize (object);
}

static void
gbp_new_file_popover_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GbpNewFilePopover *self = GBP_NEW_FILE_POPOVER(object);

  switch (prop_id)
    {
    case PROP_DIRECTORY:
      g_value_set_object (value, gbp_new_file_popover_get_directory (self));
      break;

    case PROP_FILE_TYPE:
      g_value_set_enum (value, gbp_new_file_popover_get_file_type (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

/**
 * gbp_new_file_popover_set_property:
 * @object: (in): a #GObject.
 * @prop_id: (in): The property identifier.
 * @value: (in): The given property.
 * @pspec: (in): a #ParamSpec.
 *
 * Set a given #GObject property.
 */
static void
gbp_new_file_popover_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GbpNewFilePopover *self = GBP_NEW_FILE_POPOVER(object);

  switch (prop_id)
    {
    case PROP_DIRECTORY:
      gbp_new_file_popover_set_directory (self, g_value_get_object (value));
      break;

    case PROP_FILE_TYPE:
      gbp_new_file_popover_set_file_type (self, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
gbp_new_file_popover_class_init (GbpNewFilePopoverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkPopoverClass *popover_class = GTK_POPOVER_CLASS (klass);

  object_class->finalize = gbp_new_file_popover_finalize;
  object_class->get_property = gbp_new_file_popover_get_property;
  object_class->set_property = gbp_new_file_popover_set_property;

  popover_class->closed = gbp_new_file_popover_closed;

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

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/project-tree/gbp-new-file-popover.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpNewFilePopover, button);
  gtk_widget_class_bind_template_child (widget_class, GbpNewFilePopover, entry);
  gtk_widget_class_bind_template_child (widget_class, GbpNewFilePopover, message);
  gtk_widget_class_bind_template_child (widget_class, GbpNewFilePopover, title);
}

static void
gbp_new_file_popover_init (GbpNewFilePopover *self)
{
  self->file_type = G_FILE_TYPE_REGULAR;

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (self->entry,
                           "activate",
                           G_CALLBACK (gbp_new_file_popover_entry_activate),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->entry,
                           "changed",
                           G_CALLBACK (gbp_new_file_popover_entry_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->button,
                           "clicked",
                           G_CALLBACK (gbp_new_file_popover_button_clicked),
                           self,
                           G_CONNECT_SWAPPED);
}

GFileType
gbp_new_file_popover_get_file_type (GbpNewFilePopover *self)
{
  g_return_val_if_fail (GBP_IS_NEW_FILE_POPOVER (self), 0);

  return self->file_type;
}

void
gbp_new_file_popover_set_file_type (GbpNewFilePopover *self,
                                    GFileType          file_type)
{
  g_return_if_fail (GBP_IS_NEW_FILE_POPOVER (self));
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
gbp_new_file_popover_set_directory (GbpNewFilePopover *self,
                                    GFile             *directory)
{
  g_return_if_fail (GBP_IS_NEW_FILE_POPOVER (self));
  g_return_if_fail (G_IS_FILE (directory));

  if (g_set_object (&self->directory, directory))
    {
      const gchar *path;

      path = gtk_editable_get_text (GTK_EDITABLE (self->entry));
      gbp_new_file_popover_check_exists (self, directory, path);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DIRECTORY]);
    }
}

/**
 * gbp_new_file_popover_get_directory:
 *
 * Returns: (transfer none) (nullable): a #GFile or %NULL.
 */
GFile *
gbp_new_file_popover_get_directory (GbpNewFilePopover *self)
{
  g_return_val_if_fail (GBP_IS_NEW_FILE_POPOVER (self), NULL);

  return self->directory;
}

void
gbp_new_file_popover_display_async (GbpNewFilePopover   *self,
                                    IdeTree             *tree,
                                    IdeTreeNode         *node,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_return_if_fail (GBP_IS_NEW_FILE_POPOVER (self));
  g_return_if_fail (IDE_IS_TREE (tree));
  g_return_if_fail (IDE_IS_TREE_NODE (node));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (self->task == NULL);

  self->task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (self->task, gbp_new_file_popover_display_async);

  ide_tree_expand_node (tree, node);
  ide_tree_show_popover_at_node (tree, node, GTK_POPOVER (self));
}

GFile *
gbp_new_file_popover_display_finish (GbpNewFilePopover  *self,
                                     GAsyncResult       *result,
                                     GError            **error)
{
  g_return_val_if_fail (GBP_IS_NEW_FILE_POPOVER (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}
