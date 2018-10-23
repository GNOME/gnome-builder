/* gbp-glade-workbench-addin.c
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

#define G_LOG_DOMAIN "gbp-glade-workbench-addin"

#include "gbp-glade-view.h"
#include "gbp-glade-workbench-addin.h"

struct _GbpGladeWorkbenchAddin
{
  GObject parent_instance;
  IdeWorkbench *workbench;
};

typedef struct
{
  GFile        *file;
  GbpGladeView *view;
} LocateView;

static gchar *
gbp_glade_workbench_addin_get_id (IdeWorkbenchAddin *addin)
{
  return g_strdup ("glade");
}

static gboolean
gbp_glade_workbench_addin_can_open (IdeWorkbenchAddin *addin,
                                    IdeUri            *uri,
                                    const gchar       *content_type,
                                    gint              *priority)
{
  const gchar *path;

  g_assert (GBP_IS_GLADE_WORKBENCH_ADDIN (addin));
  g_assert (priority != NULL);

  path = ide_uri_get_path (uri);

  if (g_strcmp0 (content_type, "application/x-gtk-builder") == 0 ||
      g_strcmp0 (content_type, "application/x-designer") == 0 ||
      (path && g_str_has_suffix (path, ".ui")))
    {
      *priority = -100;
      return TRUE;
    }

  return FALSE;
}

static void
gbp_glade_workbench_addin_open_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  GbpGladeView *view = (GbpGladeView *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTask) task = user_data;
  GladeProject *project;
  GList *toplevels;

  g_assert (GBP_IS_GLADE_VIEW (view));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!gbp_glade_view_load_file_finish (view, result, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  project = gbp_glade_view_get_project (view);
  toplevels = glade_project_toplevels (project);

  /* Select the first toplevel so that we don't start with a non-existant
   * selection. Otherwise, the panels look empty.
   */
  if (toplevels != NULL)
    glade_project_selection_set (project, toplevels->data, TRUE);

  ide_task_return_boolean (task, TRUE);
}

static void
locate_view (GtkWidget *view,
             gpointer   user_data)
{
  LocateView *locate = user_data;
  GFile *file;

  g_assert (IDE_IS_LAYOUT_VIEW (view));
  g_assert (locate != NULL);

  if (locate->view != NULL)
    return;

  if (!GBP_IS_GLADE_VIEW (view))
    return;

  file = gbp_glade_view_get_file (GBP_GLADE_VIEW (view));
  if (g_file_equal (file, locate->file))
    locate->view = GBP_GLADE_VIEW (view);
}

static void
gbp_glade_workbench_addin_open_async (IdeWorkbenchAddin     *addin,
                                      IdeUri                *uri,
                                      const gchar           *content_type,
                                      IdeWorkbenchOpenFlags  flags,
                                      GCancellable          *cancellable,
                                      GAsyncReadyCallback    callback,
                                      gpointer               user_data)
{
  GbpGladeWorkbenchAddin *self = (GbpGladeWorkbenchAddin *)addin;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GFile) file = NULL;
  IdePerspective *editor;
  GbpGladeView *view;
  LocateView locate = { 0 };

  g_assert (GBP_IS_GLADE_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (self->workbench));
  g_assert (uri != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_glade_workbench_addin_open_async);

  editor = ide_workbench_get_perspective_by_name (self->workbench, "editor");
  file = ide_uri_to_file (uri);

  /* First try to find an existing view for the file */
  locate.file = file;
  ide_workbench_views_foreach (self->workbench, locate_view, &locate);
  if (locate.view != NULL)
    {
      ide_workbench_focus (self->workbench, GTK_WIDGET (locate.view));
      ide_task_return_boolean (task, TRUE);
      return;
    }

  view = gbp_glade_view_new ();
  gtk_container_add (GTK_CONTAINER (editor), GTK_WIDGET (view));
  gtk_widget_show (GTK_WIDGET (view));

  gbp_glade_view_load_file_async (view,
                                  file,
                                  cancellable,
                                  gbp_glade_workbench_addin_open_cb,
                                  g_steal_pointer (&task));

  ide_workbench_focus (self->workbench, GTK_WIDGET (view));
}

static gboolean
gbp_glade_workbench_addin_open_finish (IdeWorkbenchAddin  *addin,
                                       GAsyncResult       *result,
                                       GError            **error)
{
  g_assert (GBP_IS_GLADE_WORKBENCH_ADDIN (addin));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
gbp_glade_workbench_addin_load (IdeWorkbenchAddin *addin,
                                IdeWorkbench      *workbench)
{
  GbpGladeWorkbenchAddin *self = (GbpGladeWorkbenchAddin *)addin;

  g_assert (GBP_IS_GLADE_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = workbench;
}

static void
gbp_glade_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                  IdeWorkbench      *workbench)
{
  GbpGladeWorkbenchAddin *self = (GbpGladeWorkbenchAddin *)addin;

  g_assert (GBP_IS_GLADE_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = NULL;
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->get_id = gbp_glade_workbench_addin_get_id;
  iface->load = gbp_glade_workbench_addin_load;
  iface->unload = gbp_glade_workbench_addin_unload;
  iface->can_open = gbp_glade_workbench_addin_can_open;
  iface->open_async = gbp_glade_workbench_addin_open_async;
  iface->open_finish = gbp_glade_workbench_addin_open_finish;
}

G_DEFINE_TYPE_WITH_CODE (GbpGladeWorkbenchAddin, gbp_glade_workbench_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN, workbench_addin_iface_init))

static void
gbp_glade_workbench_addin_class_init (GbpGladeWorkbenchAddinClass *klass)
{
}

static void
gbp_glade_workbench_addin_init (GbpGladeWorkbenchAddin *self)
{
}
