/* gbp-flatpak-dependency-updater.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-flatpak-dependency-updater"

#include "gbp-flatpak-dependency-updater.h"
#include "gbp-flatpak-download-stage.h"
#include "gbp-flatpak-manifest.h"
#include "gbp-flatpak-runtime.h"
#include "gbp-flatpak-sdk-stage.h"

struct _GbpFlatpakDependencyUpdater
{
  IdeObject parent_instance;
};

static char **
get_sdks (IdePipeline        *pipeline,
          GbpFlatpakManifest *manifest)
{
  const char * const *exts;
  IdeRuntime *runtime;
  GPtrArray *ar;

  g_assert (GBP_IS_FLATPAK_MANIFEST (manifest));

  ar = g_ptr_array_new ();

  if ((runtime = ide_pipeline_get_runtime (pipeline)) &&
      GBP_IS_FLATPAK_RUNTIME (runtime))
    {
      g_auto(GStrv) refs = gbp_flatpak_runtime_get_refs (GBP_FLATPAK_RUNTIME (runtime));

      for (guint i = 0; refs[i]; i++)
        g_ptr_array_add (ar, g_strdup (refs[i]));
    }

  if ((exts = gbp_flatpak_manifest_get_sdk_extensions (manifest)))
    {
      for (guint i = 0; exts[i]; i++)
        g_ptr_array_add (ar, g_strdup (exts[i]));
    }

  g_ptr_array_add (ar, NULL);

  return (char **)g_ptr_array_free (ar, FALSE);
}

static void
find_download_stage_cb (gpointer data,
                        gpointer user_data)
{
  GbpFlatpakDownloadStage **stage = user_data;

  g_assert (IDE_IS_PIPELINE_STAGE (data));
  g_assert (stage != NULL);

  if (GBP_IS_FLATPAK_DOWNLOAD_STAGE (data))
    *stage = data;
}

static void
gbp_flatpak_dependency_updater_update_cb (GObject      *object,
                                          GAsyncResult *result,
                                          gpointer      user_data)
{
  IdeBuildManager *manager = (IdeBuildManager *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_BUILD_MANAGER (manager));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_build_manager_rebuild_finish (manager, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);
}

static void
gbp_flatpak_dependency_updater_update_async (IdeDependencyUpdater *updater,
                                             GCancellable         *cancellable,
                                             GAsyncReadyCallback   callback,
                                             gpointer              user_data)
{
  GbpFlatpakDependencyUpdater *self = (GbpFlatpakDependencyUpdater *)updater;
  GbpFlatpakDownloadStage *stage = NULL;
  g_autoptr(IdeTask) task = NULL;
  IdePipeline *pipeline;
  IdeBuildManager *manager;
  IdeContext *context;
  IdeConfig *config;

  g_assert (GBP_IS_FLATPAK_DEPENDENCY_UPDATER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_flatpak_dependency_updater_update_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  context = ide_object_get_context (IDE_OBJECT (self));
  g_assert (IDE_IS_CONTEXT (context));

  manager = ide_build_manager_from_context (context);
  g_assert (IDE_IS_BUILD_MANAGER (manager));

  pipeline = ide_build_manager_get_pipeline (manager);
  g_assert (!pipeline || IDE_IS_PIPELINE (pipeline));

  if (pipeline == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "Cannot update flatpak dependencies until build pipeline is initialized");
      return;
    }

  /* Find the downloads stage and tell it to download updates one time */
  ide_pipeline_foreach_stage (pipeline, find_download_stage_cb, &stage);

  if (stage == NULL)
    {
      /* Synthesize success if they weren't using flatpak. */
      ide_task_return_boolean (task, TRUE);
      return;
    }

  gbp_flatpak_download_stage_force_update (stage);

  config = ide_pipeline_get_config (pipeline);
  if (GBP_IS_FLATPAK_MANIFEST (config))
    {
      g_auto(GStrv) sdks = get_sdks (pipeline, GBP_FLATPAK_MANIFEST (config));
      g_autoptr(GbpFlatpakSdkStage) sdk_stage = NULL;

      /* Add a stage to update SDKs */
      sdk_stage = gbp_flatpak_sdk_stage_new ((const char * const *)sdks);
      ide_pipeline_stage_set_transient (IDE_PIPELINE_STAGE (sdk_stage), TRUE);
      ide_pipeline_attach (pipeline, IDE_PIPELINE_PHASE_DOWNLOADS, 0, IDE_PIPELINE_STAGE (sdk_stage));
    }

  /* Ensure downloads and everything past it is invalidated */
  ide_pipeline_invalidate_phase (pipeline, IDE_PIPELINE_PHASE_DOWNLOADS);

  /* Start building all the way up to the project configure so that
   * the user knows if the updates broke their configuration or anything.
   */
  ide_build_manager_rebuild_async (manager,
                                   IDE_PIPELINE_PHASE_CONFIGURE,
                                   NULL,
                                   NULL,
                                   gbp_flatpak_dependency_updater_update_cb,
                                   g_steal_pointer (&task));
}

static gboolean
gbp_flatpak_dependency_updater_update_finish (IdeDependencyUpdater  *updater,
                                              GAsyncResult          *result,
                                              GError               **error)
{
  g_assert (GBP_IS_FLATPAK_DEPENDENCY_UPDATER (updater));
  g_assert (IDE_IS_TASK (result));
  g_assert (ide_task_is_valid (IDE_TASK (result), updater));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
dependency_udpater_iface_init (IdeDependencyUpdaterInterface *iface)
{
  iface->update_async = gbp_flatpak_dependency_updater_update_async;
  iface->update_finish = gbp_flatpak_dependency_updater_update_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpFlatpakDependencyUpdater,
                         gbp_flatpak_dependency_updater,
                         IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_DEPENDENCY_UPDATER,
                                                dependency_udpater_iface_init))

static void
gbp_flatpak_dependency_updater_class_init (GbpFlatpakDependencyUpdaterClass *klass)
{
}

static void
gbp_flatpak_dependency_updater_init (GbpFlatpakDependencyUpdater *self)
{
}
