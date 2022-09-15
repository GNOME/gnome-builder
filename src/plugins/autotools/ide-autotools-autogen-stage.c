/* ide-autotools-autogen-stage.c
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-autotools-autogen-stage"

#include "ide-autotools-autogen-stage.h"

struct _IdeAutotoolsAutogenStage
{
  IdePipelineStage parent_instance;

  char *srcdir;
};

G_DEFINE_FINAL_TYPE (IdeAutotoolsAutogenStage, ide_autotools_autogen_stage, IDE_TYPE_PIPELINE_STAGE)

enum {
  PROP_0,
  PROP_SRCDIR,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
ide_autotools_autogen_stage_wait_check_cb (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (IDE_IS_TASK (task));

  if (!ide_subprocess_wait_check_finish (subprocess, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);
}

static void
ide_autotools_autogen_stage_build_async (IdePipelineStage    *stage,
                                         IdePipeline         *pipeline,
                                         GCancellable        *cancellable,
                                         GAsyncReadyCallback  callback,
                                         gpointer             user_data)
{
  IdeAutotoolsAutogenStage *self = (IdeAutotoolsAutogenStage *)stage;
  g_autofree char *autogen_path = NULL;
  g_autoptr(IdeRunContext) run_context = NULL;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_AUTOTOOLS_AUTOGEN_STAGE (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_autotools_autogen_stage_build_async);

  autogen_path = g_build_filename (self->srcdir, "autogen.sh", NULL);

  run_context = ide_run_context_new ();
  ide_pipeline_prepare_run_context (pipeline, run_context);
  ide_run_context_set_cwd (run_context, self->srcdir);

  if (g_file_test (autogen_path, G_FILE_TEST_IS_REGULAR))
    {
      ide_run_context_append_argv (run_context, autogen_path);
      ide_run_context_setenv (run_context, "NOCONFIGURE", "1");
    }
  else
    {
      ide_run_context_append_args (run_context, IDE_STRV_INIT ("autoreconf", "-fiv"));
    }

  if (!(launcher = ide_run_context_end (run_context, &error)))
    IDE_GOTO (handle_error);

  ide_pipeline_attach_pty (pipeline, launcher);

  if (!(subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, &error)))
    IDE_GOTO (handle_error);

  ide_pipeline_stage_log_subprocess (stage, subprocess);

  ide_subprocess_wait_check_async (subprocess,
                                   cancellable,
                                   ide_autotools_autogen_stage_wait_check_cb,
                                   g_steal_pointer (&task));

  IDE_EXIT;

handle_error:
  ide_task_return_error (task, g_steal_pointer (&error));

  IDE_EXIT;
}

static gboolean
ide_autotools_autogen_stage_build_finish (IdePipelineStage  *stage,
                                          GAsyncResult      *result,
                                          GError           **error)
{
  g_assert (IDE_IS_AUTOTOOLS_AUTOGEN_STAGE (stage));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
ide_autotools_autogen_stage_finalize (GObject *object)
{
  IdeAutotoolsAutogenStage *self = (IdeAutotoolsAutogenStage *)object;

  g_clear_pointer (&self->srcdir, g_free);

  G_OBJECT_CLASS (ide_autotools_autogen_stage_parent_class)->finalize (object);
}

static void
ide_autotools_autogen_stage_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  IdeAutotoolsAutogenStage *self = (IdeAutotoolsAutogenStage *)object;

  switch (prop_id)
    {
    case PROP_SRCDIR:
      self->srcdir = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_autotools_autogen_stage_class_init (IdeAutotoolsAutogenStageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdePipelineStageClass *stage_class = IDE_PIPELINE_STAGE_CLASS (klass);

  object_class->finalize = ide_autotools_autogen_stage_finalize;
  object_class->set_property = ide_autotools_autogen_stage_set_property;

  stage_class->build_async = ide_autotools_autogen_stage_build_async;
  stage_class->build_finish = ide_autotools_autogen_stage_build_finish;

  properties [PROP_SRCDIR] =
    g_param_spec_string ("srcdir", NULL, NULL, NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_autotools_autogen_stage_init (IdeAutotoolsAutogenStage *self)
{
}
