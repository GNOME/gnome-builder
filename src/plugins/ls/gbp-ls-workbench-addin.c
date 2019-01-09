/* gbp-ls-workbench-addin.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-ls-workbench-addin"

#include "config.h"

#include "gbp-ls-workbench-addin.h"
#include "gbp-ls-view.h"

struct _GbpLsWorkbenchAddin
{
  GObject       parent_instance;
  IdeWorkbench *workbench;
};

typedef struct
{
  GFile     *file;
  GbpLsView *view;
} LocateView;

static gchar *
gbp_ls_workbench_addin_get_id (IdeWorkbenchAddin *addin)
{
  return g_strdup ("directory");
}

static gboolean
gbp_ls_workbench_addin_can_open (IdeWorkbenchAddin *addin,
                                 IdeUri            *uri,
                                 const gchar       *content_type,
                                 gint              *priority)
{
  g_assert (GBP_IS_LS_WORKBENCH_ADDIN (addin));
  g_assert (priority != NULL);

  if (g_strcmp0 (content_type, "inode/directory") == 0)
    {
      *priority = -100;
      return TRUE;
    }

  return FALSE;
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

  if (!GBP_IS_LS_VIEW (view))
    return;

  file = gbp_ls_view_get_directory (GBP_LS_VIEW (view));
  if (g_file_equal (file, locate->file))
    locate->view = GBP_LS_VIEW (view);
}

static void
gbp_ls_workbench_addin_open_async (IdeWorkbenchAddin     *addin,
                                   IdeUri                *uri,
                                   const gchar           *content_type,
                                   IdeWorkbenchOpenFlags  flags,
                                   GCancellable          *cancellable,
                                   GAsyncReadyCallback    callback,
                                   gpointer               user_data)
{
  GbpLsWorkbenchAddin *self = (GbpLsWorkbenchAddin *)addin;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GFile) file = NULL;
  IdePerspective *editor;
  GbpLsView *view;
  LocateView locate = { 0 };

  g_assert (GBP_IS_LS_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (self->workbench));
  g_assert (uri != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_ls_workbench_addin_open_async);

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

  view = g_object_new (GBP_TYPE_LS_VIEW,
                       "close-on-activate", TRUE,
                       "directory", file,
                       "visible", TRUE,
                       NULL);
  gtk_container_add (GTK_CONTAINER (editor), GTK_WIDGET (view));

  ide_task_return_boolean (task, TRUE);
}

static gboolean
gbp_ls_workbench_addin_open_finish (IdeWorkbenchAddin  *addin,
                                    GAsyncResult       *result,
                                    GError            **error)
{
  g_assert (GBP_IS_LS_WORKBENCH_ADDIN (addin));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
gbp_ls_workbench_addin_load (IdeWorkbenchAddin *addin,
                             IdeWorkbench      *workbench)
{
  GBP_LS_WORKBENCH_ADDIN (addin)->workbench = workbench;
}

static void
gbp_ls_workbench_addin_unload (IdeWorkbenchAddin *addin,
                               IdeWorkbench      *workbench)
{
  GBP_LS_WORKBENCH_ADDIN (addin)->workbench = NULL;
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->get_id = gbp_ls_workbench_addin_get_id;
  iface->can_open = gbp_ls_workbench_addin_can_open;
  iface->open_async = gbp_ls_workbench_addin_open_async;
  iface->open_finish = gbp_ls_workbench_addin_open_finish;
  iface->load = gbp_ls_workbench_addin_load;
  iface->unload = gbp_ls_workbench_addin_unload;
}

G_DEFINE_TYPE_WITH_CODE (GbpLsWorkbenchAddin, gbp_ls_workbench_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN, workbench_addin_iface_init))

static void
gbp_ls_workbench_addin_class_init (GbpLsWorkbenchAddinClass *klass)
{
}

static void
gbp_ls_workbench_addin_init (GbpLsWorkbenchAddin *self)
{
}
