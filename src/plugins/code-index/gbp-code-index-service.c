/* gbp-code-index-service.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-code-index-service"

#include "config.h"

#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>
#include <libide-code.h>
#include <libide-foundry.h>
#include <libide-projects.h>
#include <libide-vcs.h>
#include <libpeas.h>

#include "gbp-code-index-executor.h"
#include "gbp-code-index-plan.h"
#include "gbp-code-index-service.h"
#include "indexer-info.h"

#define DELAY_FOR_INDEXING_MSEC 500

struct _GbpCodeIndexService
{
  IdeObject          parent_instance;

  IdeNotification   *notif;
  IdeCodeIndexIndex *index;
  GCancellable      *cancellable;

  guint              queued_source;

  guint              build_inhibit : 1;
  guint              needs_indexing : 1;
  guint              indexing : 1;
  guint              started : 1;
  guint              paused : 1;
};

typedef struct
{
  IdeCodeIndexIndex *index;
  GFile *workdir;
  GFile *indexdir;
} LoadIndexes;

enum {
  PROP_0,
  PROP_PAUSED,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpCodeIndexService, gbp_code_index_service, IDE_TYPE_OBJECT)

static void     gbp_code_index_service_index_async    (GbpCodeIndexService  *self,
                                                       GCancellable         *cancellable,
                                                       GAsyncReadyCallback   callback,
                                                       gpointer              user_data);
static gboolean gbp_code_index_service_index_finish   (GbpCodeIndexService   *self,
                                                       GAsyncResult          *result,
                                                       GError              **error);
static void     gbp_code_index_service_reload_indexes (GbpCodeIndexService  *self);

static GParamSpec *properties [N_PROPS];

static void
load_indexes_free (LoadIndexes *state)
{
  g_clear_object (&state->index);
  g_clear_object (&state->indexdir);
  g_clear_object (&state->workdir);
  g_slice_free (LoadIndexes, state);
}

static void
update_notification (GbpCodeIndexService *self)
{
  gboolean visible;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_SERVICE (self));

  visible = self->indexing || self->paused;

  if (ide_object_is_root (IDE_OBJECT (self->notif)) && visible)
    ide_notification_attach (self->notif, IDE_OBJECT (self));
  else if (!ide_object_is_root (IDE_OBJECT (self->notif)) && !visible)
    ide_notification_withdraw (self->notif);
}

static gchar *
gbp_code_index_service_repr (IdeObject *object)
{
  GbpCodeIndexService *self = (GbpCodeIndexService *)object;

  return g_strdup_printf ("%s started=%d paused=%d",
                          G_OBJECT_TYPE_NAME (self), self->started, self->paused);
}

static void
gbp_code_index_service_destroy (IdeObject *object)
{
  GbpCodeIndexService *self = (GbpCodeIndexService *)object;

  if (self->started)
    gbp_code_index_service_stop (self);

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  g_clear_handle_id (&self->queued_source, g_source_remove);

  ide_clear_and_destroy_object (&self->index);

  if (self->notif)
    {
      ide_notification_withdraw (self->notif);
      g_clear_object (&self->notif);
    }

  IDE_OBJECT_CLASS (gbp_code_index_service_parent_class)->destroy (object);
}

static void
gbp_code_index_service_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GbpCodeIndexService *self = GBP_CODE_INDEX_SERVICE (object);

  switch (prop_id)
    {
    case PROP_PAUSED:
      g_value_set_boolean (value, gbp_code_index_service_get_paused (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_code_index_service_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GbpCodeIndexService *self = GBP_CODE_INDEX_SERVICE (object);

  switch (prop_id)
    {
    case PROP_PAUSED:
      gbp_code_index_service_set_paused (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_code_index_service_class_init (GbpCodeIndexServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  object_class->get_property = gbp_code_index_service_get_property;
  object_class->set_property = gbp_code_index_service_set_property;

  i_object_class->destroy = gbp_code_index_service_destroy;
  i_object_class->repr = gbp_code_index_service_repr;

  properties [PROP_PAUSED] =
    g_param_spec_boolean ("paused",
                          "Paused",
                          "If the service is paused",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_code_index_service_init (GbpCodeIndexService *self)
{
  g_autoptr(GIcon) icon = NULL;

  icon = g_themed_icon_new ("media-playback-pause-symbolic");

  self->notif = ide_notification_new ();
  ide_notification_set_id (self->notif, "org.gnome.builder.code-index");
  ide_notification_set_title (self->notif, _("Indexing Source Code"));
  ide_notification_set_body (self->notif, _("Search, diagnostics, and autocompletion may be limited until complete."));
  ide_notification_set_has_progress (self->notif, TRUE);
  ide_notification_set_progress (self->notif, 0);
  ide_notification_add_button (self->notif, NULL, icon, "context.workbench.code-index.paused");

  self->index = ide_code_index_index_new (IDE_OBJECT (self));
}

static void
gbp_code_index_service_index_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  GbpCodeIndexService *self = (GbpCodeIndexService *)object;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_SERVICE (self));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!gbp_code_index_service_index_finish (self, result, &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Code indexing failed: %s", error->message);
    }
}

static gboolean
gbp_code_index_service_queue_index_cb (gpointer user_data)
{
  GbpCodeIndexService *self = user_data;
  g_autoptr(IdeContext) context = NULL;
  IdeBuildManager *build_manager;
  IdePipeline *pipeline;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_SERVICE (self));

  self->queued_source = 0;

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  self->cancellable = g_cancellable_new ();

  if (!ide_object_in_destruction (IDE_OBJECT (self)) &&
      (context = ide_object_ref_context (IDE_OBJECT (self))) &&
      ide_context_has_project (context) &&
      (build_manager = ide_build_manager_from_context (context)) &&
      (pipeline = ide_build_manager_get_pipeline (build_manager)) &&
      ide_pipeline_has_configured (pipeline))
    gbp_code_index_service_index_async (self,
                                        self->cancellable,
                                        gbp_code_index_service_index_cb,
                                        NULL);

  IDE_RETURN (G_SOURCE_REMOVE);
}

static void
gbp_code_index_service_queue_index (GbpCodeIndexService *self)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_SERVICE (self));

  self->needs_indexing = TRUE;

  if (self->indexing || self->paused)
    return;

  g_clear_handle_id (&self->queued_source, g_source_remove);
  self->queued_source = g_timeout_add (DELAY_FOR_INDEXING_MSEC,
                                       gbp_code_index_service_queue_index_cb,
                                       self);
}

static void
gbp_code_index_service_pause (GbpCodeIndexService *self)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_SERVICE (self));

  self->paused = TRUE;

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  g_clear_handle_id (&self->queued_source, g_source_remove);

  if (!ide_object_in_destruction (IDE_OBJECT (self)))
    update_notification (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PAUSED]);
}

static void
gbp_code_index_service_unpause (GbpCodeIndexService *self)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_SERVICE (self));

  self->paused = FALSE;

  if (!ide_object_in_destruction (IDE_OBJECT (self)))
    {
      gbp_code_index_service_queue_index (self);
      update_notification (self);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PAUSED]);
}

static void
gbp_code_index_service_execute_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  GbpCodeIndexExecutor *executor = (GbpCodeIndexExecutor *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_EXECUTOR (executor));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!gbp_code_index_executor_execute_finish (executor, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  ide_object_destroy (IDE_OBJECT (executor));

  IDE_EXIT;
}

static void
gbp_code_index_service_load_flags_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  GbpCodeIndexPlan *plan = (GbpCodeIndexPlan *)object;
  g_autoptr(GbpCodeIndexExecutor) executor = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  GbpCodeIndexService *self;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_PLAN (plan));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!gbp_code_index_plan_load_flags_finish (plan, result, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (ide_task_return_error_if_cancelled (task))
    IDE_EXIT;

  self = ide_task_get_source_object (task);
  g_assert (GBP_IS_CODE_INDEX_SERVICE (self));

  executor = gbp_code_index_executor_new (plan);
  ide_object_append (IDE_OBJECT (self), IDE_OBJECT (executor));

  gbp_code_index_executor_execute_async (executor,
                                         self->notif,
                                         ide_task_get_cancellable (task),
                                         gbp_code_index_service_execute_cb,
                                         g_object_ref (task));

  IDE_EXIT;
}

static void
gbp_code_index_service_cull_index_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  GbpCodeIndexPlan *plan = (GbpCodeIndexPlan *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_PLAN (plan));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!gbp_code_index_plan_cull_indexed_finish (plan, result, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (ide_task_return_error_if_cancelled (task))
    IDE_EXIT;

  context = ide_task_get_task_data (task);
  g_assert (IDE_IS_CONTEXT (context));

  gbp_code_index_plan_load_flags_async (plan,
                                        context,
                                        ide_task_get_cancellable (task),
                                        gbp_code_index_service_load_flags_cb,
                                        g_object_ref (task));

  IDE_EXIT;
}

static void
gbp_code_index_service_populate_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  GbpCodeIndexPlan *plan = (GbpCodeIndexPlan *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_PLAN (plan));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!gbp_code_index_plan_populate_finish (plan, result, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (ide_task_return_error_if_cancelled (task))
    IDE_EXIT;

  context = ide_task_get_task_data (task);
  g_assert (IDE_IS_CONTEXT (context));

  gbp_code_index_plan_cull_indexed_async (plan,
                                          context,
                                          ide_task_get_cancellable (task),
                                          gbp_code_index_service_cull_index_cb,
                                          g_object_ref (task));

  IDE_EXIT;
}

static void
gbp_code_index_service_index_async (GbpCodeIndexService *self,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_autoptr(GbpCodeIndexPlan) plan = NULL;
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_SERVICE (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (cancellable == NULL)
    g_warning ("Attempt to index without a valid cancellable. This will affect pausibility.");

  self->indexing = TRUE;
  self->needs_indexing = FALSE;

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_code_index_service_index_async);

  if (ide_task_return_error_if_cancelled (task))
    IDE_EXIT;

  context = ide_object_ref_context (IDE_OBJECT (self));
  g_assert (IDE_IS_CONTEXT (context));

  ide_task_set_task_data (task, g_object_ref (context), g_object_unref);

  plan = gbp_code_index_plan_new ();

  gbp_code_index_plan_populate_async (plan,
                                      context,
                                      cancellable,
                                      gbp_code_index_service_populate_cb,
                                      g_steal_pointer (&task));

  update_notification (self);

  IDE_EXIT;
}

static gboolean
gbp_code_index_service_index_finish (GbpCodeIndexService  *self,
                                     GAsyncResult         *result,
                                     GError              **error)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_SERVICE (self));
  g_assert (IDE_IS_TASK (result));

  self->indexing = FALSE;

  if (!ide_object_in_destruction (IDE_OBJECT (self)))
    {
      update_notification (self);
      gbp_code_index_service_reload_indexes (self);
    }

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
gbp_code_index_service_buffer_saved_cb (GbpCodeIndexService *self,
                                        IdeBuffer           *buffer,
                                        IdeBufferManager    *buffer_manager)
{
  GtkSourceLanguage *lang;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_SERVICE (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));

  /*
   * Only update the index if the file save will result in a change to the
   * directory's index. We determine that by if an indexer is available.
   */

  if ((lang = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (buffer))))
    {
      const gchar *lang_id = gtk_source_language_get_id (lang);
      PeasEngine *engine = peas_engine_get_default ();
      guint n_items = g_list_model_get_n_items (G_LIST_MODEL (engine));

      for (guint i = 0; i < n_items; i++)
        {
          g_autoptr(PeasPluginInfo) plugin_info = g_list_model_get_item (G_LIST_MODEL (engine), i);
          const gchar *languages;

          if (!peas_plugin_info_is_loaded (plugin_info))
            continue;

          /* Not exact check, but good enough for now */
          languages = peas_plugin_info_get_external_data (plugin_info, "Code-Indexer-Languages");
          if (languages != NULL && strstr (languages, lang_id) != NULL)
            {
              gbp_code_index_service_queue_index (self);
              break;
            }
        }
    }
}

static void
gbp_code_index_service_build_started_cb (GbpCodeIndexService *self,
                                         IdePipeline         *pipeline,
                                         IdeBuildManager     *build_manager)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_SERVICE (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  /* If we are starting a new build that is going to ensure that we reach to
   * the configure phase (or further), then delay any index building until
   * after that operation completes. There is no need to compete for resources
   * while building (especially if indexing might fail anyway).
   */
  if (ide_pipeline_get_requested_phase (pipeline) >= IDE_PIPELINE_PHASE_CONFIGURE)
    {
      self->build_inhibit = TRUE;
      g_cancellable_cancel (self->cancellable);
      g_clear_object (&self->cancellable);
    }
}

static void
gbp_code_index_service_build_failed_cb (GbpCodeIndexService *self,
                                        IdePipeline         *pipeline,
                                        IdeBuildManager     *build_manager)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_SERVICE (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  self->build_inhibit = FALSE;
}

static void
gbp_code_index_service_build_finished_cb (GbpCodeIndexService *self,
                                          IdePipeline         *pipeline,
                                          IdeBuildManager     *build_manager)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_SERVICE (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  /*
   * If we paused building due to inhibition while building, then we need to
   * possibly restore the build process and queue a new indexing.
   */

  if (self->build_inhibit)
    {
      self->build_inhibit = FALSE;

      if (ide_pipeline_has_configured (pipeline))
        gbp_code_index_service_queue_index (self);
    }
}

static void
gbp_code_index_service_vcs_changed_cb (GbpCodeIndexService *self,
                                       IdeVcs              *vcs)
{
  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_SERVICE (self));
  g_assert (IDE_IS_VCS (vcs));

  /* Possibly switched branches, queue re-indexing */
  gbp_code_index_service_queue_index (self);

  IDE_EXIT;
}

static void
gbp_code_index_service_file_trashed_cb (GbpCodeIndexService *self,
                                        GFile               *file,
                                        IdeProject          *project)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_SERVICE (self));
  g_assert (G_IS_FILE (file));
  g_assert (IDE_IS_PROJECT (project));

  gbp_code_index_service_queue_index (self);
}

static void
gbp_code_index_service_file_renamed_cb (GbpCodeIndexService *self,
                                        GFile               *src_file,
                                        GFile               *dst_file,
                                        IdeProject          *project)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_SERVICE (self));
  g_assert (G_IS_FILE (src_file));
  g_assert (G_IS_FILE (dst_file));
  g_assert (IDE_IS_PROJECT (project));

  gbp_code_index_service_queue_index (self);
}

static void
gbp_code_index_service_load_indexes_cb (GFile     *directory,
                                        GPtrArray *file_infos,
                                        gpointer   user_data)
{
  LoadIndexes *state = user_data;
  g_autoptr(GFile) source_directory = NULL;

  g_assert (G_IS_FILE (directory));
  g_assert (state != NULL);

  if (!g_file_equal (directory, state->indexdir))
    {
      g_autofree gchar *relative = NULL;

      relative = g_file_get_relative_path (state->indexdir, directory);
      source_directory = g_file_get_child (state->workdir, relative);
    }
  else
    {
      source_directory = g_object_ref (state->workdir);
    }

  ide_code_index_index_load (state->index,
                             directory,
                             source_directory,
                             NULL,
                             NULL);
}

static void
gbp_code_index_service_load_indexes (IdeTask      *task,
                                     gpointer      source_object,
                                     gpointer      task_data,
                                     GCancellable *cancellable)
{
  LoadIndexes *state = task_data;

  g_assert (IDE_IS_TASK (task));
  g_assert (GBP_IS_CODE_INDEX_SERVICE (source_object));
  g_assert (state != NULL);
  g_assert (IDE_IS_CODE_INDEX_INDEX (state->index));
  g_assert (G_IS_FILE (state->workdir));
  g_assert (G_IS_FILE (state->indexdir));

  ide_g_file_walk (state->indexdir,
                   NULL,
                   cancellable,
                   gbp_code_index_service_load_indexes_cb,
                   state);

  ide_task_return_boolean (task, TRUE);
}

static void
gbp_code_index_service_reload_indexes (GbpCodeIndexService *self)
{
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(IdeTask) task = NULL;
  LoadIndexes *state;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_SERVICE (self));

  if (!(context = ide_object_ref_context (IDE_OBJECT (self))))
    return;

  state = g_slice_new0 (LoadIndexes);
  state->index = g_object_ref (self->index);
  state->workdir = ide_context_ref_workdir (context);
  state->indexdir = ide_context_cache_file (context, "code-index", NULL);

  task = ide_task_new (self, NULL, NULL, NULL);
  ide_task_set_source_tag (task, gbp_code_index_service_reload_indexes);
  ide_task_set_task_data (task, state, load_indexes_free);
  ide_task_run_in_thread (task, gbp_code_index_service_load_indexes);
}

void
gbp_code_index_service_start (GbpCodeIndexService *self)
{
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(GFile) index_dir = NULL;
  IdeBufferManager *buffer_manager;
  IdeBuildManager *build_manager;
  IdeProject *project;
  IdeVcs *vcs;
  gboolean has_index;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (GBP_IS_CODE_INDEX_SERVICE (self));
  g_return_if_fail (self->started == FALSE);
  g_return_if_fail (!ide_object_in_destruction (IDE_OBJECT (self)));

  self->started = TRUE;

  if (!(context = ide_object_ref_context (IDE_OBJECT (self))))
    {
      g_warning ("Attempt to start code-index service without access to context");
      IDE_EXIT;
    }

  buffer_manager = ide_buffer_manager_from_context (context);

  g_signal_connect_object (buffer_manager,
                           "buffer-saved",
                           G_CALLBACK (gbp_code_index_service_buffer_saved_cb),
                           self,
                           G_CONNECT_SWAPPED);

  build_manager = ide_build_manager_from_context (context);

  g_signal_connect_object (build_manager,
                           "build-failed",
                           G_CALLBACK (gbp_code_index_service_build_failed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (build_manager,
                           "build-finished",
                           G_CALLBACK (gbp_code_index_service_build_finished_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (build_manager,
                           "build-started",
                           G_CALLBACK (gbp_code_index_service_build_started_cb),
                           self,
                           G_CONNECT_SWAPPED);

  vcs = ide_vcs_from_context (context);

  g_signal_connect_object (vcs,
                           "changed",
                           G_CALLBACK (gbp_code_index_service_vcs_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  project = ide_project_from_context (context);

  g_signal_connect_object (project,
                           "file-trashed",
                           G_CALLBACK (gbp_code_index_service_file_trashed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (project,
                           "file-renamed",
                           G_CALLBACK (gbp_code_index_service_file_renamed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  index_dir = ide_context_cache_file (context, "code-index", NULL);
  has_index = g_file_query_exists (index_dir, NULL);

  if (!self->paused)
    {
      /*
       * We only want to immediately start indexing at startup if the project
       * does not yet have an index. Otherwise, we want to wait for a user
       * action to cause the indexes to be rebuilt so that we don't risk
       * annoying the user with build actions.
       */
      if (!has_index && !ide_build_manager_get_busy (build_manager))
        gbp_code_index_service_queue_index (self);
    }

  gbp_code_index_service_reload_indexes (self);

  IDE_EXIT;
}

void
gbp_code_index_service_stop (GbpCodeIndexService *self)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (GBP_IS_CODE_INDEX_SERVICE (self));

  if (!self->started)
    return;

  self->started = FALSE;

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  g_clear_handle_id (&self->queued_source, g_source_remove);

  if (self->notif)
    {
      ide_notification_withdraw (self->notif);
      g_clear_object (&self->notif);
    }
}

gboolean
gbp_code_index_service_get_paused (GbpCodeIndexService *self)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (GBP_IS_CODE_INDEX_SERVICE (self), FALSE);

  return self->paused;
}

void
gbp_code_index_service_set_paused (GbpCodeIndexService *self,
                                   gboolean             paused)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (GBP_IS_CODE_INDEX_SERVICE (self));

  paused = !!paused;

  if (paused != self->paused)
    {
      if (paused)
        gbp_code_index_service_pause (self);
      else
        gbp_code_index_service_unpause (self);
    }
}

static IdeCodeIndexer *
create_indexer (GbpCodeIndexService *self,
                const gchar         *module_name)
{
  PeasEngine *engine = peas_engine_get_default ();
  PeasPluginInfo *plugin_info;

  g_return_val_if_fail (module_name != NULL, NULL);

  if ((plugin_info = peas_engine_get_plugin_info (engine, module_name)) &&
      peas_plugin_info_is_loaded (plugin_info))
    {
      GObject *exten;

      exten = peas_engine_create_extension (peas_engine_get_default (),
                                            plugin_info,
                                            IDE_TYPE_CODE_INDEXER,
                                            NULL);

      if (IDE_IS_OBJECT (exten))
        ide_object_append (IDE_OBJECT (self), IDE_OBJECT (exten));

      return IDE_CODE_INDEXER (exten);
    }

  return NULL;
}

IdeCodeIndexer *
gbp_code_index_service_get_indexer (GbpCodeIndexService *self,
                                    const gchar         *lang_id,
                                    const gchar         *path)
{
  g_autoptr(GPtrArray) indexers = NULL;

  g_return_val_if_fail (GBP_IS_CODE_INDEX_SERVICE (self), NULL);

  indexers = collect_indexer_info ();

  if (lang_id != NULL)
    {
      for (guint i = 0; i < indexers->len; i++)
        {
          const IndexerInfo *info = g_ptr_array_index (indexers, i);

          if (info->lang_ids == NULL)
            continue;

          for (guint j = 0; info->lang_ids[j]; j++)
            {
              if (g_str_equal (lang_id, info->lang_ids[j]))
                return create_indexer (self, info->module_name);
            }
        }
    }

  if (path != NULL)
    {
      g_autofree gchar *name = g_path_get_basename (path);
      g_autofree gchar *reversed = g_utf8_strreverse (name, -1);

      for (guint i = 0; i < indexers->len; i++)
        {
          const IndexerInfo *info = g_ptr_array_index (indexers, i);

          if (indexer_info_matches (info, name, reversed, NULL))
            return create_indexer (self, info->module_name);
        }
    }

  return NULL;
}

GbpCodeIndexService *
gbp_code_index_service_from_context (IdeContext *context)
{
  GbpCodeIndexService *ret;

  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  if (!(ret = ide_context_peek_child_typed (context, GBP_TYPE_CODE_INDEX_SERVICE)))
    {
      g_autoptr(GbpCodeIndexService) self = NULL;

      self = g_object_new (GBP_TYPE_CODE_INDEX_SERVICE,
                           "parent", context,
                           NULL);
      gbp_code_index_service_start (self);
      ret = ide_context_peek_child_typed (context, GBP_TYPE_CODE_INDEX_SERVICE);
    }

  return ret;
}

IdeCodeIndexIndex *
gbp_code_index_service_get_index (GbpCodeIndexService *self)
{
  g_return_val_if_fail (GBP_IS_CODE_INDEX_SERVICE (self), NULL);

  return self->index;
}
