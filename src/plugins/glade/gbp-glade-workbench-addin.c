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
  GHashTable *catalog_paths;
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
on_build_pipeline_changed_cb (GbpGladeWorkbenchAddin *self,
                              GParamSpec             *pspec,
                              IdeBuildManager        *build_manager)
{
  IdeBuildPipeline *pipeline;
  GHashTableIter iter;
  const gchar *key;

  g_assert (GBP_IS_GLADE_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  g_hash_table_iter_init (&iter, self->catalog_paths);
  while (g_hash_table_iter_next (&iter, (gpointer *)&key, NULL))
    {
      g_debug ("Removing catalogs from: %s", key);
      glade_catalog_remove_path (key);
      g_hash_table_iter_remove (&iter);
    }

  /* Get path from the installation prefix in the build runtime. */

  pipeline = ide_build_manager_get_pipeline (build_manager);

  if (pipeline != NULL)
    {
      IdeConfiguration *config = ide_build_pipeline_get_configuration (pipeline);
      IdeRuntime *runtime = ide_build_pipeline_get_runtime (pipeline);
      g_autoptr(GFile) translated = NULL;
      g_autoptr(GFile) catalog_file = NULL;

      if (config != NULL)
        {
          const gchar *prefix = ide_configuration_get_prefix (config);
          g_autofree gchar *path = g_build_filename (prefix, "share/glade/catalogs", NULL);

          if (!g_hash_table_contains (self->catalog_paths, path))
            {
              g_debug ("Adding catalogs from installation prefix: %s", path);
              glade_catalog_add_path (path);
              g_hash_table_insert (self->catalog_paths, g_steal_pointer (&path), NULL);
            }
        }

      /*
       * Get the path to the default installation path
       * (/usr/share/glade/catalogs) for catalogs in the runtime. We just
       * assume the natural /usr here since we deal with alternate installation
       * paths above.
       */

      if (runtime != NULL)
        {
          const gchar *path;

          catalog_file = g_file_new_for_path ("/usr/share/glade/catalogs");
          translated = ide_runtime_translate_file (runtime, catalog_file);
          path = g_file_peek_path (translated);

          if (!g_hash_table_contains (self->catalog_paths, path))
            {
              g_debug ("Adding catalogs from runtime: %s", path);
              glade_catalog_add_path (path);
              g_hash_table_insert (self->catalog_paths, g_strdup (path), NULL);
            }
        }
    }
}

static void
gbp_glade_workbench_addin_load (IdeWorkbenchAddin *addin,
                                IdeWorkbench      *workbench)
{
  GbpGladeWorkbenchAddin *self = (GbpGladeWorkbenchAddin *)addin;
  IdeBuildManager *build_manager;
  IdeContext *context;

  g_assert (GBP_IS_GLADE_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = workbench;
  self->catalog_paths = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  /*
   * We want to watch the build pipeline for changes to the current
   * runtime, so that we can update the glade catalog paths based
   * on the installation prefix.
   *
   * For example, if we get "/opt/gnome" as a prefix, we want to get
   * the catalog path from "/opt/gnome/share/glade/catalogs/". Or if we
   * get "/app" we want "/app/share/glade/catalogs/". We also need to
   * translate the path to something we can locate on the host, in case
   * the runtime is a foreign mount.
   */

  context = ide_workbench_get_context (workbench);
  build_manager = ide_context_get_build_manager (context);

  g_signal_connect_object (build_manager,
                           "notify::pipeline",
                           G_CALLBACK (on_build_pipeline_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  /* Update catalogs */
  on_build_pipeline_changed_cb (self, NULL, build_manager);
}

static void
gbp_glade_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                  IdeWorkbench      *workbench)
{
  GbpGladeWorkbenchAddin *self = (GbpGladeWorkbenchAddin *)addin;
  IdeBuildManager *build_manager;
  IdeContext *context;
  const gchar *path;
  GHashTableIter iter;

  g_assert (GBP_IS_GLADE_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  context = ide_workbench_get_context (workbench);
  build_manager = ide_context_get_build_manager (context);

  g_signal_handlers_disconnect_by_func (build_manager,
                                        G_CALLBACK (on_build_pipeline_changed_cb),
                                        self);

  g_hash_table_iter_init (&iter, self->catalog_paths);
  while (g_hash_table_iter_next (&iter, (gpointer *)&path, NULL))
    {
      g_debug ("Removing catalogs from: %s", path);
      glade_catalog_remove_path (path);
      g_hash_table_iter_remove (&iter);
    }

  g_clear_pointer (&self->catalog_paths, g_hash_table_unref);

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
