/* ide-autotools-autogen-stage.c
 *
 * Copyright © 2016 Christian Hergert <chergert@redhat.com>
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
 */

#define G_LOG_DOMAIN "ide-autotools-autogen-stage"

#include "ide-autotools-autogen-stage.h"

struct _IdeAutotoolsAutogenStage
{
  IdeBuildStage parent_instance;

  gchar *srcdir;
};

G_DEFINE_TYPE (IdeAutotoolsAutogenStage, ide_autotools_autogen_stage, IDE_TYPE_BUILD_STAGE)

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
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_TASK (task));

  if (!ide_subprocess_wait_check_finish (subprocess, result, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);
}

static void
ide_autotools_autogen_stage_execute_async (IdeBuildStage       *stage,
                                           IdeBuildPipeline    *pipeline,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data)
{
  IdeAutotoolsAutogenStage *self = (IdeAutotoolsAutogenStage *)stage;
  g_autofree gchar *autogen_path = NULL;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(GTask) task = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_AUTOTOOLS_AUTOGEN_STAGE (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_autotools_autogen_stage_execute_async);

  autogen_path = g_build_filename (self->srcdir, "autogen.sh", NULL);

  launcher = ide_build_pipeline_create_launcher (pipeline, &error);

  if (launcher == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  ide_subprocess_launcher_set_cwd (launcher, self->srcdir);

  if (g_file_test (autogen_path, G_FILE_TEST_IS_REGULAR))
    {
      ide_subprocess_launcher_push_argv (launcher, autogen_path);
      ide_subprocess_launcher_setenv (launcher, "NOCONFIGURE", "1", TRUE);
    }
  else
    {
      ide_subprocess_launcher_push_argv (launcher, "autoreconf");
      ide_subprocess_launcher_push_argv (launcher, "-fiv");
    }

  subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, &error);

  if (subprocess == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  ide_build_stage_log_subprocess (stage, subprocess);

  ide_subprocess_wait_check_async (subprocess,
                                   cancellable,
                                   ide_autotools_autogen_stage_wait_check_cb,
                                   g_steal_pointer (&task));
}

static gboolean
ide_autotools_autogen_stage_execute_finish (IdeBuildStage  *stage,
                                            GAsyncResult   *result,
                                            GError        **error)
{
  g_assert (IDE_IS_AUTOTOOLS_AUTOGEN_STAGE (stage));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean (G_TASK (result), error);
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
  IdeBuildStageClass *stage_class = IDE_BUILD_STAGE_CLASS (klass);

  object_class->finalize = ide_autotools_autogen_stage_finalize;
  object_class->set_property = ide_autotools_autogen_stage_set_property;

  stage_class->execute_async = ide_autotools_autogen_stage_execute_async;
  stage_class->execute_finish = ide_autotools_autogen_stage_execute_finish;

  properties [PROP_SRCDIR] =
    g_param_spec_string ("srcdir", NULL, NULL, NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  
  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_autotools_autogen_stage_init (IdeAutotoolsAutogenStage *self)
{
}
