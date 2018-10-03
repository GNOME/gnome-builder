/* ide-git-pipeline-addin.c
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

#define G_LOG_DOMAIN "ide-git-pipeline-addin"

#include <glib/gi18n.h>

#include "ide-git-pipeline-addin.h"
#include "ide-git-vcs.h"

struct _IdeGitPipelineAddin
{
  IdeObject parent_instance;
  guint has_run : 1;
};

static void
submodule_status_cb (GObject      *object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  g_autoptr(IdeBuildStage) stage = user_data;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *stdout_buf = NULL;
  IdeLineReader reader;
  const gchar *line;
  gsize line_len;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_BUILD_STAGE_LAUNCHER (stage));

  if (!ide_subprocess_communicate_utf8_finish (subprocess, result, &stdout_buf, NULL, &error))
    {
      ide_build_stage_log (stage,
                           IDE_BUILD_LOG_STDERR,
                           error->message,
                           -1);
      goto failure;
    }

  ide_line_reader_init (&reader, stdout_buf, -1);
  while ((line = ide_line_reader_next (&reader, &line_len)))
    {
      /* If we find a line starting with -, it isn't initialized
       * and needs a submodule-init/update.
       */
      if (stdout_buf[0] == '-')
        {
          ide_build_stage_set_completed (stage, FALSE);
          goto unpause;
        }
    }

failure:
  ide_build_stage_set_completed (stage, TRUE);

unpause:
  ide_build_stage_unpause (stage);
}

static void
submodule_update_query_cb (IdeGitPipelineAddin   *self,
                           IdeBuildPipeline      *pipeline,
                           GCancellable          *cancellable,
                           IdeBuildStageLauncher *stage)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(GError) error = NULL;
  IdeContext *context;
  IdeVcs *vcs;
  GFile *workdir;

  g_assert (IDE_IS_GIT_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (IDE_IS_BUILD_STAGE_LAUNCHER (stage));

  /* Short-circuit if we've already run */
  if (self->has_run)
    {
      ide_build_stage_set_completed (IDE_BUILD_STAGE (stage), TRUE);
      return;
    }

  self->has_run = TRUE;

  if (!ide_application_has_network (IDE_APPLICATION_DEFAULT))
    {
      ide_build_stage_log (IDE_BUILD_STAGE (stage),
                           IDE_BUILD_LOG_STDERR,
                           _("Network is not available, skipping submodule update"),
                           -1);
      ide_build_stage_set_completed (IDE_BUILD_STAGE (stage), TRUE);
      return;
    }

  /* We need to run "git submodule status" to see if there are any
   * lines that are prefixed with - (meaning they have not yet been
   * initialized).
   *
   * We only do a git submodule init/update if that is the case, otherwise
   * dependencies are updated with the dependency updater.
   */

  context = ide_object_get_context (IDE_OBJECT (self));
  vcs = ide_context_get_vcs (context);
  workdir = ide_vcs_get_working_directory (vcs);

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);
  ide_subprocess_launcher_push_argv (launcher, "git");
  ide_subprocess_launcher_push_argv (launcher, "submodule");
  ide_subprocess_launcher_push_argv (launcher, "status");
  ide_subprocess_launcher_set_cwd (launcher, g_file_peek_path (workdir));
  ide_subprocess_launcher_set_clear_env (launcher, FALSE);

  if (!(subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, &error)))
    {
      ide_build_stage_log (IDE_BUILD_STAGE (stage),
                           IDE_BUILD_LOG_STDERR,
                           error->message,
                           -1);
      ide_build_stage_set_completed (IDE_BUILD_STAGE (stage), TRUE);
      return;
    }

  ide_build_stage_pause (IDE_BUILD_STAGE (stage));

  ide_subprocess_communicate_utf8_async (subprocess,
                                         NULL,
                                         cancellable,
                                         submodule_status_cb,
                                         g_object_ref (stage));
}

static void
ide_git_pipeline_addin_load (IdeBuildPipelineAddin *addin,
                             IdeBuildPipeline      *pipeline)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(GError) error = NULL;
  IdeBuildStage *stage;
  IdeContext *context;
  IdeVcs *vcs;
  GFile *workdir;
  guint stage_id;

  g_assert (IDE_IS_GIT_PIPELINE_ADDIN (addin));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));

  context = ide_object_get_context (IDE_OBJECT (addin));
  vcs = ide_context_get_vcs (context);
  workdir = ide_vcs_get_working_directory (vcs);

  /* Ignore everything if this isn't a git-based repository */
  if (!IDE_IS_GIT_VCS (vcs))
    return;

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                          G_SUBPROCESS_FLAGS_STDERR_PIPE);
  ide_subprocess_launcher_set_clear_env (launcher, FALSE);
  ide_subprocess_launcher_set_cwd (launcher, g_file_peek_path (workdir));
  ide_subprocess_launcher_push_argv (launcher, "sh");
  ide_subprocess_launcher_push_argv (launcher, "-c");
  ide_subprocess_launcher_push_argv (launcher, "git submodule init && git submodule update");

  stage_id = ide_build_pipeline_connect_launcher (pipeline,
                                                  IDE_BUILD_PHASE_DOWNLOADS,
                                                  100,
                                                  launcher);
  stage = ide_build_pipeline_get_stage_by_id (pipeline, stage_id);
  ide_build_pipeline_addin_track (addin, stage_id);

  ide_build_stage_launcher_set_ignore_exit_status (IDE_BUILD_STAGE_LAUNCHER (stage), TRUE);
  ide_build_stage_set_name (stage, _("Initialize git submodules"));

  g_signal_connect_object (stage,
                           "query",
                           G_CALLBACK (submodule_update_query_cb),
                           addin,
                           G_CONNECT_SWAPPED);
}

static void
build_pipeline_addin_iface_init (IdeBuildPipelineAddinInterface *iface)
{
  iface->load = ide_git_pipeline_addin_load;
}

G_DEFINE_TYPE_WITH_CODE (IdeGitPipelineAddin, ide_git_pipeline_addin, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_PIPELINE_ADDIN,
                                                build_pipeline_addin_iface_init))

static void
ide_git_pipeline_addin_class_init (IdeGitPipelineAddinClass *klass)
{
}

static void
ide_git_pipeline_addin_init (IdeGitPipelineAddin *self)
{
}
