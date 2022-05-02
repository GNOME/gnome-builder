/* gbp-rename-file-popover.c
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

#define G_LOG_DOMAIN "gbp-rename-file-popover"

#include <glib/gi18n.h>
#include <libide-gui.h>
#include <string.h>

#include "gbp-rename-file-popover.h"

struct _GbpRenameFilePopover
{
  GtkPopover    parent_instance;

  GCancellable *cancellable;
  GFile        *file;
  IdeTask      *task;

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
  N_PROPS
};

enum {
  RENAME_FILE,
  N_SIGNALS
};

G_DEFINE_FINAL_TYPE (GbpRenameFilePopover, gbp_rename_file_popover, GTK_TYPE_POPOVER)

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

GFile *
gbp_rename_file_popover_get_file (GbpRenameFilePopover *self)
{
  g_return_val_if_fail (GBP_IS_RENAME_FILE_POPOVER (self), NULL);

  return self->file;
}

static void
gbp_rename_file_popover_set_file (GbpRenameFilePopover *self,
                                  GFile                *file)
{
  g_return_if_fail (GBP_IS_RENAME_FILE_POPOVER (self));
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
          gtk_editable_set_text (GTK_EDITABLE (self->entry), name);
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FILE]);
    }
}

static void
gbp_rename_file_popover_set_is_directory (GbpRenameFilePopover *self,
                                          gboolean              is_directory)
{
  g_return_if_fail (GBP_IS_RENAME_FILE_POPOVER (self));

  is_directory = !!is_directory;

  if (is_directory != self->is_directory)
    {
      self->is_directory = is_directory;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_IS_DIRECTORY]);
    }
}

static void
gbp_rename_file_popover__file_query_info (GObject      *object,
                                          GAsyncResult *result,
                                          gpointer      user_data)
{
  GFile *file = (GFile *)object;
  g_autoptr(GFileInfo) file_info = NULL;
  g_autoptr(GbpRenameFilePopover) self = user_data;
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
gbp_rename_file_popover__entry_changed (GbpRenameFilePopover *self,
                                        GtkEntry             *entry)
{
  g_autoptr(GFile) parent = NULL;
  g_autoptr(GFile) file = NULL;
  g_autofree gchar *stripped = NULL;
  const gchar *text;

  g_assert (GBP_IS_RENAME_FILE_POPOVER (self));
  g_assert (GTK_IS_ENTRY (entry));
  g_assert (self->file != NULL);
  g_assert (G_IS_FILE (self->file));

  gtk_widget_set_sensitive (GTK_WIDGET (self->button), FALSE);
  gtk_label_set_label (self->message, NULL);

  text = gtk_editable_get_text (GTK_EDITABLE (entry));
  if (ide_str_empty0 (text))
    return;

  /* make sure to strip so that warnings (eg. "file already exists") are
   * consistents with the final behavior (creating the file). */
  stripped = g_strstrip (g_strdup (text));

  if (self->cancellable)
    {
      g_cancellable_cancel (self->cancellable);
      g_clear_object (&self->cancellable);
    }

  self->cancellable = g_cancellable_new ();

  parent = g_file_get_parent (self->file);
  file = g_file_get_child (parent, stripped);

  g_file_query_info_async (file,
                           G_FILE_ATTRIBUTE_STANDARD_TYPE,
                           G_FILE_QUERY_INFO_NONE,
                           G_PRIORITY_DEFAULT,
                           self->cancellable,
                           gbp_rename_file_popover__file_query_info,
                           g_object_ref (self));
}

static void
gbp_rename_file_popover__entry_activate (GbpRenameFilePopover *self,
                                         GtkEntry             *entry)
{
  g_assert (GBP_IS_RENAME_FILE_POPOVER (self));
  g_assert (GTK_IS_ENTRY (entry));

  if (gtk_widget_get_sensitive (GTK_WIDGET (self->button)))
    gtk_widget_activate (GTK_WIDGET (self->button));
}

static gboolean
select_range_in_idle_cb (GtkEntry *entry)
{
  const gchar *name;
  const gchar *dot;

  g_assert (GTK_IS_ENTRY (entry));

  name = gtk_editable_get_text (GTK_EDITABLE (entry));

  if ((dot = strrchr (name, '.')))
    {
      gsize len = g_utf8_strlen (name, dot - name);
      gtk_editable_select_region (GTK_EDITABLE (entry), 0, len);
    }

  return G_SOURCE_REMOVE;
}

static void
gbp_rename_file_popover__entry_focus_in_event (GbpRenameFilePopover    *self,
                                               GtkEventControllerFocus *focus)
{
  g_assert (GBP_IS_RENAME_FILE_POPOVER (self));
  g_assert (GTK_IS_EVENT_CONTROLLER_FOCUS (focus));
  g_assert (GTK_IS_ENTRY (self->entry));

  g_idle_add_full (G_PRIORITY_DEFAULT,
                   (GSourceFunc) select_range_in_idle_cb,
                   g_object_ref (self->entry),
                   g_object_unref);
}

static void
gbp_rename_file_popover__button_clicked (GbpRenameFilePopover *self,
                                         GtkButton            *button)
{
  g_autoptr(GFile) file = NULL;
  g_autoptr(GFile) parent = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autofree gchar *stripped = NULL;
  const gchar *path;

  g_assert (GBP_IS_RENAME_FILE_POPOVER (self));
  g_assert (GTK_IS_BUTTON (button));
  g_assert (self->file != NULL);
  g_assert (G_IS_FILE (self->file));

  path = gtk_editable_get_text (GTK_EDITABLE (self->entry));
  if (ide_str_empty0 (path))
    return;

  stripped = g_strstrip (g_strdup (path));

  parent = g_file_get_parent (self->file);
  file = g_file_get_child (parent, stripped);

  /* only activate once */
  gtk_widget_set_sensitive (GTK_WIDGET (self->button), FALSE);

  g_signal_emit (self, signals [RENAME_FILE], 0, self->file, file);

  /* Complete our async op */
  if ((task = g_steal_pointer (&self->task)))
    ide_task_return_pointer (task, g_steal_pointer (&file), g_object_unref);
}

static void
gbp_rename_file_popover_closed (GtkPopover *popover)
{
  g_autoptr(IdeTask) task = NULL;
  GbpRenameFilePopover *self = (GbpRenameFilePopover *)popover;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_RENAME_FILE_POPOVER (self));

  if ((task = g_steal_pointer (&self->task)))
    ide_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_CANCELLED,
                               "The popover was cancelled");
}

static void
gbp_rename_file_popover_finalize (GObject *object)
{
  GbpRenameFilePopover *self = (GbpRenameFilePopover *)object;

  if (self->cancellable != NULL)
    {
      if (!g_cancellable_is_cancelled (self->cancellable))
        g_cancellable_cancel (self->cancellable);
      g_clear_object (&self->cancellable);
    }

  g_clear_object (&self->file);

  g_assert (self->task == NULL);

  G_OBJECT_CLASS (gbp_rename_file_popover_parent_class)->finalize (object);
}

static void
gbp_rename_file_popover_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  GbpRenameFilePopover *self = GBP_RENAME_FILE_POPOVER (object);

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
gbp_rename_file_popover_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  GbpRenameFilePopover *self = GBP_RENAME_FILE_POPOVER (object);

  switch (prop_id)
    {
    case PROP_FILE:
      gbp_rename_file_popover_set_file (self, g_value_get_object (value));
      break;

    case PROP_IS_DIRECTORY:
      gbp_rename_file_popover_set_is_directory (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_rename_file_popover_class_init (GbpRenameFilePopoverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkPopoverClass *popover_class = GTK_POPOVER_CLASS (klass);

  object_class->finalize = gbp_rename_file_popover_finalize;
  object_class->get_property = gbp_rename_file_popover_get_property;
  object_class->set_property = gbp_rename_file_popover_set_property;

  popover_class->closed = gbp_rename_file_popover_closed;

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

  g_object_class_install_properties (object_class, N_PROPS, properties);

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

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/project-tree/gbp-rename-file-popover.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpRenameFilePopover, button);
  gtk_widget_class_bind_template_child (widget_class, GbpRenameFilePopover, entry);
  gtk_widget_class_bind_template_child (widget_class, GbpRenameFilePopover, label);
  gtk_widget_class_bind_template_child (widget_class, GbpRenameFilePopover, message);
}

static void
gbp_rename_file_popover_init (GbpRenameFilePopover *self)
{
  GtkEventController *controller;

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (self->entry,
                           "changed",
                           G_CALLBACK (gbp_rename_file_popover__entry_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->entry,
                           "activate",
                           G_CALLBACK (gbp_rename_file_popover__entry_activate),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->button,
                           "clicked",
                           G_CALLBACK (gbp_rename_file_popover__button_clicked),
                           self,
                           G_CONNECT_SWAPPED);

  controller = gtk_event_controller_focus_new ();
  g_signal_connect_object (controller,
                           "enter",
                           G_CALLBACK (gbp_rename_file_popover__entry_focus_in_event),
                           self,
                           G_CONNECT_SWAPPED | G_CONNECT_AFTER);
  gtk_widget_add_controller (GTK_WIDGET (self->entry), controller);

}

void
gbp_rename_file_popover_display_async (GbpRenameFilePopover *self,
                                       IdeTree              *tree,
                                       IdeTreeNode          *node,
                                       GCancellable         *cancellable,
                                       GAsyncReadyCallback   callback,
                                       gpointer              user_data)
{
  g_autoptr(IdeTask) task = NULL;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (GBP_IS_RENAME_FILE_POPOVER (self));
  g_return_if_fail (IDE_IS_TREE (tree));
  g_return_if_fail (IDE_IS_TREE_NODE (node));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_rename_file_popover_display_async);

  if (self->task != NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "Already displayed popover");
      return;
    }

  self->task = g_steal_pointer (&task);

  ide_tree_show_popover_at_node (tree, node, GTK_POPOVER (self));
}

GFile *
gbp_rename_file_popover_display_finish (GbpRenameFilePopover  *self,
                                        GAsyncResult          *result,
                                        GError               **error)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (GBP_IS_RENAME_FILE_POPOVER (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}
