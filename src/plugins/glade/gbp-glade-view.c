/* gbp-glade-view.c
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#define G_LOG_DOMAIN "gbp-glade-view"

#include <glib/gi18n.h>

#include "gbp-glade-view.h"
#include "gbp-glade-private.h"

G_DEFINE_TYPE (GbpGladeView, gbp_glade_view, IDE_TYPE_LAYOUT_VIEW)

/**
 * gbp_glade_view_new:
 *
 * Create a new #GbpGladeView.
 *
 * Returns: (transfer full): a newly created #GbpGladeView
 */
GbpGladeView *
gbp_glade_view_new (void)
{
  return g_object_new (GBP_TYPE_GLADE_VIEW, NULL);
}

static void
gbp_glade_view_dispose (GObject *object)
{
  GbpGladeView *self = (GbpGladeView *)object;

  g_clear_object (&self->project);

  G_OBJECT_CLASS (gbp_glade_view_parent_class)->dispose (object);
}

static void
gbp_glade_view_class_init (GbpGladeViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gbp_glade_view_dispose;

  gtk_widget_class_set_css_name (widget_class, "gbpgladeview");
}

static void
gbp_glade_view_init (GbpGladeView *self)
{
  ide_layout_view_set_menu_id (IDE_LAYOUT_VIEW (self), "gbp-glade-view-menu");
  ide_layout_view_set_title (IDE_LAYOUT_VIEW (self), _("Unnamed Glade project"));
  ide_layout_view_set_icon_name (IDE_LAYOUT_VIEW (self), "glade-symbolic");
  ide_layout_view_set_menu_id (IDE_LAYOUT_VIEW (self), "gbp-glade-view-document-menu");

  self->project = glade_project_new ();
  self->designer = g_object_new (GLADE_TYPE_DESIGN_VIEW,
                                 "project", self->project,
                                 "vexpand", TRUE,
                                 "visible", TRUE,
                                 NULL);
  dzl_gtk_widget_add_style_class (GTK_WIDGET (self->designer), "glade-designer");
  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->designer));

  glade_app_add_project (self->project);

  g_object_bind_property (G_OBJECT (self->project), "modified", self, "modified", G_BINDING_DEFAULT);

  _gbp_glade_view_init_actions (self);
}

/**
 * gbp_glade_view_get_project:
 *
 * Returns: (transfer none): A #GladeProject or %NULL
 */
GladeProject *
gbp_glade_view_get_project (GbpGladeView *self)
{
  g_return_val_if_fail (GBP_IS_GLADE_VIEW (self), NULL);

  return self->project;
}

static void
gbp_glade_view_load_file_map_cb (GladeDesignView *designer,
                                 IdeTask         *task)
{
  g_autofree gchar *name = NULL;
  GbpGladeView *self;
  const gchar *path;
  GFile *file;

  g_assert (GLADE_IS_DESIGN_VIEW (designer));
  g_assert (IDE_IS_TASK (task));
  g_assert (gtk_widget_get_mapped (GTK_WIDGET (designer)));

  self = ide_task_get_source_object (task);
  file = ide_task_get_task_data (task);

  g_signal_handlers_disconnect_by_func (self->designer,
                                        G_CALLBACK (gbp_glade_view_load_file_map_cb),
                                        task);

  if (!g_file_is_native (file))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVALID_FILENAME,
                                 "File must be a local file");
      return;
    }

  path = g_file_peek_path (file);

  if (!glade_project_load_from_file (self->project, path))
    ide_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Failed to load glade project");
  else
    ide_task_return_boolean (task, TRUE);

  name = glade_project_get_name (self->project);
  ide_layout_view_set_title (IDE_LAYOUT_VIEW (self), name);
}

void
gbp_glade_view_load_file_async (GbpGladeView        *self,
                                GFile               *file,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;

  g_return_if_fail (GBP_IS_GLADE_VIEW (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_glade_view_load_file_async);
  ide_task_set_task_data (task, g_object_ref (file), g_object_unref);

  g_set_object (&self->file, file);

  /* We can't load the file until we have been mapped or else we see an issue
   * where toplevels cannot be parented properly. If we come across that, then
   * delay until the widget is mapped.
   */
  if (!gtk_widget_get_mapped (GTK_WIDGET (self->designer)))
    g_signal_connect_data (self->designer,
                           "map",
                           G_CALLBACK (gbp_glade_view_load_file_map_cb),
                           g_steal_pointer (&task),
                           (GClosureNotify)g_object_unref,
                           0);
  else
    gbp_glade_view_load_file_map_cb (self->designer, task);
}

gboolean
gbp_glade_view_load_file_finish (GbpGladeView  *self,
                                 GAsyncResult  *result,
                                 GError       **error)
{
  g_return_val_if_fail (GBP_IS_GLADE_VIEW (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}
