/* gbp-code-index-workbench-addin.c
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

#define G_LOG_DOMAIN "gbp-code-index-workbench-addin"

#include "config.h"

#include <dazzle.h>
#include <gtksourceview/gtksource.h>
#include <libide-foundry.h>
#include <libide-gui.h>
#include <libide-plugins.h>
#include <libide-projects.h>
#include <libide-vcs.h>

#include "gbp-code-index-workbench-addin.h"
#include "ide-code-index-builder.h"

#define DEFAULT_INDEX_TIMEOUT_SECS 5
#define MAX_TRIALS 3

struct _GbpCodeIndexWorkbenchAddin
{
  IdeObject               parent_instance;

  IdeWorkbench           *workbench;

  /* The builder to build & update index */
  IdeCodeIndexBuilder    *builder;

  /* The Index which will store all declarations */
  IdeCodeIndexIndex      *index;

  /* Queue of directories which needs to be indexed */
  GQueue                  build_queue;
  GHashTable             *build_dirs;

  GHashTable             *code_indexers;

  IdeNotification        *notif;
  GCancellable           *cancellable;

  guint                   paused : 1;
  guint                   delayed_build_reqeusted : 1;
};

typedef struct
{
  volatile gint               ref_count;
  GbpCodeIndexWorkbenchAddin *self;
  GFile                      *directory;
  guint                       n_trial;
  guint                       recursive : 1;
} BuildData;

static void gbp_code_index_workbench_addin_build (GbpCodeIndexWorkbenchAddin *self,
                                                  GFile                      *directory,
                                                  gboolean                    recursive,
                                                  guint                       n_trial);

static void
remove_source (gpointer source_id)
{
  if (source_id != NULL)
    g_source_remove (GPOINTER_TO_UINT (source_id));
}

static void
build_data_unref (BuildData *data)
{
  g_assert (data != NULL);
  g_assert (data->ref_count > 0);

  if (g_atomic_int_dec_and_test (&data->ref_count))
    {
      g_clear_object (&data->self);
      g_clear_object (&data->directory);
      g_slice_free (BuildData, data);
    }
}

static BuildData *
build_data_ref (BuildData *data)
{
  g_assert (data != NULL);
  g_assert (data->ref_count > 0);
  g_atomic_int_inc (&data->ref_count);
  return data;
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (BuildData, build_data_unref)

static void
register_notification (GbpCodeIndexWorkbenchAddin *self)
{
  g_autoptr(IdeNotification) notif = NULL;
  g_autoptr(GIcon) icon = NULL;
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_WORKBENCH_ADDIN (self));
  g_assert (self->notif == NULL);

  icon = g_icon_new_for_string ("media-playback-pause-symbolic", NULL);

  notif = ide_notification_new ();
  ide_notification_set_id (notif, "org.gnome.builder.code-index");
  ide_notification_set_title (notif, "Indexing Source Code");
  ide_notification_set_body (notif, "Search, diagnostics, and autocompletion may be limited until complete.");
  ide_notification_set_has_progress (notif, TRUE);
  ide_notification_set_progress (notif, 0);
  ide_notification_set_progress_is_imprecise (notif, TRUE);
  ide_notification_add_button (notif, NULL, icon, "code-index.paused");

  context = ide_workbench_get_context (self->workbench);
  ide_notification_attach (notif, IDE_OBJECT (context));

  self->notif = g_object_ref (notif);
}

static void
unregister_notification (GbpCodeIndexWorkbenchAddin *self)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_WORKBENCH_ADDIN (self));

  if (self->notif != NULL)
    {
      ide_notification_withdraw (self->notif);
      g_clear_object (&self->notif);
    }
}

static gboolean
delay_until_build_completes (GbpCodeIndexWorkbenchAddin *self)
{
  IdeBuildPipeline *pipeline;
  IdeBuildManager *build_manager;
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_WORKBENCH_ADDIN (self));

  if (self->delayed_build_reqeusted)
    return TRUE;

  context = ide_object_get_context (IDE_OBJECT (self));
  g_assert (IDE_IS_CONTEXT (context));

  build_manager = ide_build_manager_from_context (context);
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  pipeline = ide_build_manager_get_pipeline (build_manager);
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));

  if (pipeline == NULL || !ide_build_pipeline_has_configured (pipeline))
    {
      self->delayed_build_reqeusted = TRUE;
      return TRUE;
    }

  return FALSE;
}

static void
gbp_code_index_workbench_addin_build_cb (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  g_autoptr(GbpCodeIndexWorkbenchAddin) self = user_data;
  IdeCodeIndexBuilder *builder = (IdeCodeIndexBuilder *)object;
  g_autoptr(BuildData) bdata = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_WORKBENCH_ADDIN (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_CODE_INDEX_BUILDER (builder));

  if (ide_code_index_builder_build_finish (builder, result, &error))
    g_debug ("Finished building code index");
  else if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    g_warning ("Failed to build code index: %s", error->message);

  if (ide_object_in_destruction (IDE_OBJECT (self)))
    return;

  bdata = g_queue_pop_head (&self->build_queue);

  /*
   * If we're paused, push this item back on the queue to
   * be processed when we unpause.
   */
  if (self->paused)
    {
      g_queue_push_head (&self->build_queue, g_steal_pointer (&bdata));
      return;
    }

  if (error != NULL &&
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      gbp_code_index_workbench_addin_build (self,
                                            bdata->directory,
                                            bdata->recursive,
                                            bdata->n_trial + 1);
    }

  /* Index next directory */
  if (!g_queue_is_empty (&self->build_queue))
    {
      BuildData *peek = g_queue_peek_head (&self->build_queue);

      g_clear_object (&self->cancellable);
      self->cancellable = g_cancellable_new ();

      ide_code_index_builder_build_async (builder,
                                          peek->directory,
                                          peek->recursive,
                                          self->cancellable,
                                          gbp_code_index_workbench_addin_build_cb,
                                          g_object_ref (self));
    }
  else
    {
      unregister_notification (self);
    }
}

static gboolean
ide_code_index_serivce_push (BuildData *bdata)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (bdata != NULL);
  g_assert (GBP_IS_CODE_INDEX_WORKBENCH_ADDIN (bdata->self));
  g_assert (G_IS_FILE (bdata->directory));

  if (g_queue_is_empty (&bdata->self->build_queue))
    {
      g_queue_push_tail (&bdata->self->build_queue, build_data_ref (bdata));

      g_clear_object (&bdata->self->cancellable);
      bdata->self->cancellable = g_cancellable_new ();

      register_notification (bdata->self);

      ide_code_index_builder_build_async (bdata->self->builder,
                                          bdata->directory,
                                          bdata->recursive,
                                          bdata->self->cancellable,
                                          gbp_code_index_workbench_addin_build_cb,
                                          g_object_ref (bdata->self));
    }
  else
    {
      g_queue_push_tail (&bdata->self->build_queue, build_data_ref (bdata));
    }

  if (bdata->self->build_dirs != NULL)
    g_hash_table_remove (bdata->self->build_dirs, bdata->directory);

  return G_SOURCE_REMOVE;
}

static void
gbp_code_index_workbench_addin_build (GbpCodeIndexWorkbenchAddin *self,
                                      GFile                      *directory,
                                      gboolean                    recursive,
                                      guint                       n_trial)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_WORKBENCH_ADDIN (self));
  g_assert (G_IS_FILE (directory));

  if (n_trial > MAX_TRIALS)
    return;

  /*
   * If the build system is currently failed, then don't try to
   * do any indexing now. We'll wait for a successful build that
   * at least reaches IDE_BUILD_PHASE_CONFIGURE and then trigger
   * after that.
   */
  if (delay_until_build_completes (self))
    return;

  if (!g_hash_table_lookup (self->build_dirs, directory))
    {
      g_autoptr(BuildData) bdata = NULL;
      guint source_id;

      bdata = g_slice_new0 (BuildData);
      bdata->ref_count = 1;
      bdata->self = g_object_ref (self);
      bdata->directory = g_object_ref (directory);
      bdata->recursive = recursive;
      bdata->n_trial = n_trial;

      source_id = g_timeout_add_seconds_full (G_PRIORITY_LOW,
                                              DEFAULT_INDEX_TIMEOUT_SECS,
                                              (GSourceFunc) ide_code_index_serivce_push,
                                              g_steal_pointer (&bdata),
                                              (GDestroyNotify) build_data_unref);

      g_hash_table_insert (self->build_dirs,
                           g_object_ref (directory),
                           GUINT_TO_POINTER (source_id));
    }
}

static void
gbp_code_index_workbench_addin_vcs_changed (GbpCodeIndexWorkbenchAddin *self,
                                            IdeVcs                     *vcs)
{
  GFile *workdir;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_VCS (vcs));

  workdir = ide_vcs_get_workdir (vcs);
  gbp_code_index_workbench_addin_build (self, workdir, TRUE, 1);
}

static void
gbp_code_index_workbench_addin_buffer_saved (GbpCodeIndexWorkbenchAddin *self,
                                             IdeBuffer                  *buffer,
                                             IdeBufferManager           *buffer_manager)
{
  g_autofree gchar *file_name = NULL;
  GFile *file;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  file = ide_buffer_get_file (buffer);
  file_name = g_file_get_uri (file);

  if (!!gbp_code_index_workbench_addin_get_code_indexer (self, file_name))
    {
      g_autoptr(GFile) parent = NULL;

      parent = g_file_get_parent (file);
      gbp_code_index_workbench_addin_build (self, parent, FALSE, 1);
    }
}

static void
gbp_code_index_workbench_addin_file_trashed (GbpCodeIndexWorkbenchAddin *self,
                                             GFile                      *file,
                                             IdeProject                 *project)
{
  g_autofree gchar *file_name = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_WORKBENCH_ADDIN (self));
  g_assert (G_IS_FILE (file));

  file_name = g_file_get_uri (file);

  if (!!gbp_code_index_workbench_addin_get_code_indexer (self, file_name))
    {
      g_autoptr(GFile) parent = NULL;

      parent = g_file_get_parent (file);
      gbp_code_index_workbench_addin_build (self, parent, FALSE, 1);
    }
}

static void
gbp_code_index_workbench_addin_file_renamed (GbpCodeIndexWorkbenchAddin *self,
                                             GFile                      *src_file,
                                             GFile                      *dst_file,
                                             IdeProject                 *project)
{
  g_autofree gchar *src_file_name = NULL;
  g_autofree gchar *dst_file_name = NULL;
  g_autoptr(GFile) src_parent = NULL;
  g_autoptr(GFile) dst_parent = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_WORKBENCH_ADDIN (self));
  g_assert (G_IS_FILE (src_file));
  g_assert (G_IS_FILE (dst_file));

  src_file_name = g_file_get_uri (src_file);
  dst_file_name = g_file_get_uri (dst_file);

  src_parent = g_file_get_parent (src_file);
  dst_parent = g_file_get_parent (dst_file);

  if (g_file_equal (src_parent, dst_parent))
    {
      if (NULL != gbp_code_index_workbench_addin_get_code_indexer (self, src_file_name) ||
          NULL != gbp_code_index_workbench_addin_get_code_indexer (self, dst_file_name))
        gbp_code_index_workbench_addin_build (self, src_parent, FALSE, 1);
    }
  else
    {
      if (NULL != gbp_code_index_workbench_addin_get_code_indexer (self, src_file_name))
        gbp_code_index_workbench_addin_build (self, src_parent, FALSE, 1);

      if (NULL != gbp_code_index_workbench_addin_get_code_indexer (self, dst_file_name))
        gbp_code_index_workbench_addin_build (self, dst_parent, FALSE, 1);
    }
}

static void
gbp_code_index_workbench_addin_build_finished (GbpCodeIndexWorkbenchAddin *self,
                                               IdeBuildPipeline           *pipeline,
                                               IdeBuildManager            *build_manager)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  if (self->delayed_build_reqeusted &&
      ide_build_pipeline_has_configured (pipeline))
    {
      g_autoptr(GFile) workdir = NULL;
      IdeContext *context;

      self->delayed_build_reqeusted = FALSE;

      context = ide_object_get_context (IDE_OBJECT (self));
      workdir = ide_context_ref_workdir (context);

      gbp_code_index_workbench_addin_build (self, workdir, TRUE, 1);
    }
}

static void
gbp_code_index_workbench_addin_load (IdeWorkbenchAddin *addin,
                                     IdeWorkbench      *workbench)
{
  GbpCodeIndexWorkbenchAddin *self = (GbpCodeIndexWorkbenchAddin *)addin;
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = workbench;

  context = ide_workbench_get_context (workbench);
  ide_object_append (IDE_OBJECT (context), IDE_OBJECT (self));
}

static void
gbp_code_index_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                       IdeWorkbench      *workbench)
{
  GbpCodeIndexWorkbenchAddin *self = (GbpCodeIndexWorkbenchAddin *)addin;
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  g_clear_pointer (&self->build_dirs, g_hash_table_unref);
  g_clear_pointer (&self->code_indexers, g_hash_table_unref);
  ide_clear_and_destroy_object (&self->builder);
  ide_clear_and_destroy_object (&self->index);

  context = ide_workbench_get_context (workbench);
  ide_object_remove (IDE_OBJECT (context), IDE_OBJECT (self));

  self->workbench = NULL;
}

static void
gbp_code_index_workbench_addin_project_loaded (IdeWorkbenchAddin *addin,
                                               IdeProjectInfo    *project_info)
{
  GbpCodeIndexWorkbenchAddin *self = (GbpCodeIndexWorkbenchAddin *)addin;
  IdeBufferManager *bufmgr;
  IdeBuildManager *buildmgr;
  IdeContext *context;
  IdeProject *project;
  IdeVcs *vcs;
  GFile *workdir;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_PROJECT_INFO (project_info));

  context = ide_workbench_get_context (self->workbench);
  project = ide_project_from_context (context);
  bufmgr = ide_buffer_manager_from_context (context);
  buildmgr = ide_build_manager_from_context (context);
  vcs = ide_vcs_from_context (context);
  workdir = ide_vcs_get_workdir (vcs);

  self->code_indexers = g_hash_table_new_full (NULL,
                                               NULL,
                                               NULL,
                                               (GDestroyNotify)ide_object_unref_and_destroy);
  self->index = ide_code_index_index_new (IDE_OBJECT (self));
  self->builder = ide_code_index_builder_new (IDE_OBJECT (self), self->index);
  self->build_dirs = g_hash_table_new_full (g_file_hash,
                                            (GEqualFunc)g_file_equal,
                                            g_object_unref,
                                            remove_source);

  g_signal_connect_object (vcs,
                           "changed",
                           G_CALLBACK (gbp_code_index_workbench_addin_vcs_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (bufmgr,
                           "buffer-saved",
                           G_CALLBACK (gbp_code_index_workbench_addin_buffer_saved),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (buildmgr,
                           "build-finished",
                           G_CALLBACK (gbp_code_index_workbench_addin_build_finished),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (project,
                           "file-trashed",
                           G_CALLBACK (gbp_code_index_workbench_addin_file_trashed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (project,
                           "file-renamed",
                           G_CALLBACK (gbp_code_index_workbench_addin_file_renamed),
                           self,
                           G_CONNECT_SWAPPED);

  gbp_code_index_workbench_addin_build (self, workdir, TRUE, 1);
}

static void
gbp_code_index_workbench_addin_workspace_added (IdeWorkbenchAddin *addin,
                                                IdeWorkspace      *workspace)
{
  GbpCodeIndexWorkbenchAddin *self = (GbpCodeIndexWorkbenchAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  gtk_widget_insert_action_group (GTK_WIDGET (workspace),
                                  "code-index",
                                  G_ACTION_GROUP (self));
}

static void
gbp_code_index_workbench_addin_workspace_removed (IdeWorkbenchAddin *addin,
                                                  IdeWorkspace      *workspace)
{
  GbpCodeIndexWorkbenchAddin *self = (GbpCodeIndexWorkbenchAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  gtk_widget_insert_action_group (GTK_WIDGET (workspace), "code-index", NULL);
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = gbp_code_index_workbench_addin_load;
  iface->unload = gbp_code_index_workbench_addin_unload;
  iface->project_loaded = gbp_code_index_workbench_addin_project_loaded;
  iface->workspace_added = gbp_code_index_workbench_addin_workspace_added;
  iface->workspace_removed = gbp_code_index_workbench_addin_workspace_removed;
}

static void
gbp_code_index_workbench_addin_paused (GbpCodeIndexWorkbenchAddin *self,
                                       GVariant                   *state)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_WORKBENCH_ADDIN (self));

  if (state == NULL || !g_variant_is_of_type (state, G_VARIANT_TYPE_BOOLEAN))
    return;

  if (g_variant_get_boolean (state))
    gbp_code_index_workbench_addin_pause (self);
  else
    gbp_code_index_workbench_addin_unpause (self);
}

DZL_DEFINE_ACTION_GROUP (GbpCodeIndexWorkbenchAddin, gbp_code_index_workbench_addin, {
  { "paused", NULL, NULL, "false", gbp_code_index_workbench_addin_paused },
})

G_DEFINE_TYPE_WITH_CODE (GbpCodeIndexWorkbenchAddin, gbp_code_index_workbench_addin, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP,
                                                gbp_code_index_workbench_addin_init_action_group)
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN, workbench_addin_iface_init))

static void
gbp_code_index_workbench_addin_class_init (GbpCodeIndexWorkbenchAddinClass *klass)
{
}

static void
gbp_code_index_workbench_addin_init (GbpCodeIndexWorkbenchAddin *self)
{
}

void
gbp_code_index_workbench_addin_pause (GbpCodeIndexWorkbenchAddin *self)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (GBP_IS_CODE_INDEX_WORKBENCH_ADDIN (self));

  if (ide_object_in_destruction (IDE_OBJECT (self)))
    return;

  if (self->paused)
    return;

  self->paused = TRUE;

  gbp_code_index_workbench_addin_set_action_state (self,
                                                   "paused",
                                                   g_variant_new_boolean (TRUE));

  /*
   * To pause things, we need to cancel our current task. The completion of the
   * async task will check for cancelled and leave the build task for another
   * pass.
   */

  g_cancellable_cancel (self->cancellable);
}

void
gbp_code_index_workbench_addin_unpause (GbpCodeIndexWorkbenchAddin *self)
{
  BuildData *peek;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (GBP_IS_CODE_INDEX_WORKBENCH_ADDIN (self));

  if (ide_object_in_destruction (IDE_OBJECT (self)))
    return;

  if (!self->paused)
    return;

  self->paused = FALSE;

  gbp_code_index_workbench_addin_set_action_state (self,
                                                   "paused",
                                                   g_variant_new_boolean (FALSE));

  peek = g_queue_peek_head (&self->build_queue);

  if (peek != NULL)
    {
      GCancellable *cancellable;

      g_clear_object (&self->cancellable);
      self->cancellable = cancellable = g_cancellable_new ();

      ide_code_index_builder_build_async (self->builder,
                                          peek->directory,
                                          peek->recursive,
                                          cancellable,
                                          gbp_code_index_workbench_addin_build_cb,
                                          g_object_ref (self));
    }
}

/**
 * gbp_code_index_workbench_addin_get_code_indexer:
 * @self: a #GbpCodeIndexWorkbenchAddin
 * @file_name: the name of the file to index
 *
 * Gets an #IdeCodeIndexer suitable for @file_name.
 *
 * Returns: (transfer none) (nullable): an #IdeCodeIndexer or %NULL
 */
IdeCodeIndexer *
gbp_code_index_workbench_addin_get_code_indexer (GbpCodeIndexWorkbenchAddin *self,
                                                 const gchar                *file_name)
{
  GtkSourceLanguageManager *manager;
  IdeExtensionAdapter *adapter;
  GtkSourceLanguage *language;
  const gchar *lang_id;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (GBP_IS_CODE_INDEX_WORKBENCH_ADDIN (self), NULL);
  g_return_val_if_fail (file_name != NULL, NULL);

  if (self->code_indexers == NULL ||
      !(manager = gtk_source_language_manager_get_default ()) ||
      !(language = gtk_source_language_manager_guess_language (manager, file_name, NULL)) ||
      !(lang_id = gtk_source_language_get_id (language)))
    return NULL;

  lang_id = g_intern_string (lang_id);
  adapter = g_hash_table_lookup (self->code_indexers, lang_id);

  g_assert (!adapter || IDE_IS_EXTENSION_ADAPTER (adapter));

  if (adapter == NULL)
    {
      adapter = ide_extension_adapter_new (IDE_OBJECT (self),
                                           peas_engine_get_default (),
                                           IDE_TYPE_CODE_INDEXER,
                                           "Code-Indexer-Languages",
                                           lang_id);
      g_hash_table_insert (self->code_indexers, (gchar *)lang_id, adapter);
    }

  g_assert (IDE_IS_EXTENSION_ADAPTER (adapter));

  return ide_extension_adapter_get_extension (adapter);
}

GbpCodeIndexWorkbenchAddin *
gbp_code_index_workbench_addin_from_context (IdeContext *context)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  return ide_context_peek_child_typed (context, GBP_TYPE_CODE_INDEX_WORKBENCH_ADDIN);
}

IdeCodeIndexIndex *
gbp_code_index_workbench_addin_get_index (GbpCodeIndexWorkbenchAddin *self)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (GBP_IS_CODE_INDEX_WORKBENCH_ADDIN (self), NULL);

  return self->index;
}
