/* gbp-git-submodule-stage.c
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

#define G_LOG_DOMAIN "gbp-git-submodule-stage"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-code.h>
#include <libide-gui.h>
#include <libide-vcs.h>

#include "gbp-git-submodule-stage.h"

struct _GbpGitSubmoduleStage
{
  IdeBuildStageLauncher parent_instance;

  guint has_run : 1;
  guint force_update : 1;
};

G_DEFINE_TYPE (GbpGitSubmoduleStage, gbp_git_submodule_stage, IDE_TYPE_BUILD_STAGE_LAUNCHER)

GbpGitSubmoduleStage *
gbp_git_submodule_stage_new (IdeContext *context)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(GbpGitSubmoduleStage) self = NULL;
  g_autoptr(GFile) workdir = NULL;

  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  workdir = ide_context_ref_workdir (context);

  self = g_object_new (GBP_TYPE_GIT_SUBMODULE_STAGE, NULL);

  launcher = ide_subprocess_launcher_new (0);
  ide_subprocess_launcher_set_cwd (launcher, g_file_peek_path (workdir));
  ide_subprocess_launcher_set_clear_env (launcher, FALSE);
  ide_subprocess_launcher_push_argv (launcher, "sh");
  ide_subprocess_launcher_push_argv (launcher, "-c");
  ide_subprocess_launcher_push_argv (launcher, "git submodule init && git submodule update");

  ide_build_stage_launcher_set_launcher (IDE_BUILD_STAGE_LAUNCHER (self), launcher);

  return g_steal_pointer (&self);
}

static void
gbp_git_submodule_stage_query_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  g_autoptr(GbpGitSubmoduleStage) self = user_data;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *stdout_buf = NULL;
  IdeLineReader reader;
  const gchar *line;
  gsize line_len;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_GIT_SUBMODULE_STAGE (self));

  if (!ide_subprocess_communicate_utf8_finish (subprocess, result, &stdout_buf, NULL, &error))
    {
      ide_build_stage_log (IDE_BUILD_STAGE (self),
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
          ide_build_stage_set_completed (IDE_BUILD_STAGE (self), FALSE);
          goto unpause;
        }
    }

failure:
  ide_build_stage_set_completed (IDE_BUILD_STAGE (self), TRUE);

unpause:
  ide_build_stage_unpause (IDE_BUILD_STAGE (self));
}

static void
gbp_git_submodule_stage_query (IdeBuildStage    *stage,
                               IdeBuildPipeline *pipeline,
                               GPtrArray        *targets,
                               GCancellable     *cancellable)
{
  GbpGitSubmoduleStage *self = (GbpGitSubmoduleStage *)stage;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) workdir = NULL;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (GBP_IS_GIT_SUBMODULE_STAGE (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (!ide_application_has_network (IDE_APPLICATION_DEFAULT))
    {
      ide_build_stage_log (stage,
                           IDE_BUILD_LOG_STDERR,
                           _("Network is not available, skipping submodule update"),
                           -1);
      ide_build_stage_set_completed (stage, TRUE);
      IDE_EXIT;
    }

  if (self->force_update)
    {
      self->force_update = FALSE;
      self->has_run = TRUE;
      ide_build_stage_set_completed (stage, FALSE);
      IDE_EXIT;
    }

  if (self->has_run)
    {
      ide_build_stage_set_completed (stage, TRUE);
      IDE_EXIT;
    }

  self->has_run = TRUE;

  /* We need to run "git submodule status" to see if there are any
   * lines that are prefixed with - (meaning they have not yet been
   * initialized).
   *
   * We only do a git submodule init/update if that is the case, otherwise
   * dependencies are updated with the dependency updater.
   */

  context = ide_object_get_context (IDE_OBJECT (self));
  workdir = ide_context_ref_workdir (context);

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
      IDE_EXIT;
    }

  ide_build_stage_pause (IDE_BUILD_STAGE (stage));

  ide_subprocess_communicate_utf8_async (subprocess,
                                         NULL,
                                         cancellable,
                                         gbp_git_submodule_stage_query_cb,
                                         g_object_ref (self));

  IDE_EXIT;
}

static void
gbp_git_submodule_stage_class_init (GbpGitSubmoduleStageClass *klass)
{
  IdeBuildStageClass *stage_class = IDE_BUILD_STAGE_CLASS (klass);

  stage_class->query = gbp_git_submodule_stage_query;
}

static void
gbp_git_submodule_stage_init (GbpGitSubmoduleStage *self)
{
  ide_build_stage_set_name (IDE_BUILD_STAGE (self), _("Initialize git submodules"));
  ide_build_stage_launcher_set_ignore_exit_status (IDE_BUILD_STAGE_LAUNCHER (self), TRUE);
}

void
gbp_git_submodule_stage_force_update (GbpGitSubmoduleStage *self)
{
  g_return_if_fail (GBP_IS_GIT_SUBMODULE_STAGE (self));

  self->force_update = TRUE;
}
