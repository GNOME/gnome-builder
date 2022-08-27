/* ide-test.c
 *
 * Copyright 2017-2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-test"

#include "config.h"

#include <unistd.h>

#include <libide-io.h>
#include <libide-threading.h>

#include "ide-foundry-enums.h"
#include "ide-pipeline.h"
#include "ide-run-command.h"
#include "ide-run-context.h"
#include "ide-runtime.h"
#include "ide-test-private.h"

struct _IdeTest
{
  GObject        parent_instance;
  IdeRunCommand *run_command;
  IdeTestStatus  status;
};

enum {
  PROP_0,
  PROP_ICON_NAME,
  PROP_ID,
  PROP_RUN_COMMAND,
  PROP_STATUS,
  PROP_TITLE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeTest, ide_test, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
ide_test_set_status (IdeTest       *self,
                     IdeTestStatus  status)
{
  g_assert (IDE_IS_TEST (self));

  if (status != self->status)
    {
      self->status = status;

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_STATUS]);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ICON_NAME]);
    }
}

static void
ide_test_dispose (GObject *object)
{
  IdeTest *self = (IdeTest *)object;

  g_clear_object (&self->run_command);

  G_OBJECT_CLASS (ide_test_parent_class)->dispose (object);
}

static void
ide_test_get_property (GObject    *object,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  IdeTest *self = IDE_TEST (object);

  switch (prop_id)
    {
    case PROP_ICON_NAME:
      g_value_set_string (value, ide_test_get_icon_name (self));
      break;

    case PROP_ID:
      g_value_set_string (value, ide_test_get_id (self));
      break;

    case PROP_RUN_COMMAND:
      g_value_set_object (value, ide_test_get_run_command (self));
      break;

    case PROP_STATUS:
      g_value_set_enum (value, ide_test_get_status (self));
      break;

    case PROP_TITLE:
      g_value_set_string (value, ide_test_get_title (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_test_set_property (GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  IdeTest *self = IDE_TEST (object);

  switch (prop_id)
    {
    case PROP_RUN_COMMAND:
      g_set_object (&self->run_command, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_test_class_init (IdeTestClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_test_dispose;
  object_class->get_property = ide_test_get_property;
  object_class->set_property = ide_test_set_property;

  properties [PROP_ICON_NAME] =
    g_param_spec_string ("icon-name", NULL, NULL, NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_ID] =
    g_param_spec_string ("id", NULL, NULL, NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_RUN_COMMAND] =
    g_param_spec_object ("run-command", NULL, NULL,
                         IDE_TYPE_RUN_COMMAND,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_STATUS] =
    g_param_spec_enum ("status", NULL, NULL,
                       IDE_TYPE_TEST_STATUS,
                       IDE_TEST_STATUS_NONE,
                       (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL, NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_test_init (IdeTest *self)
{
}

IdeTest *
ide_test_new (IdeRunCommand *run_command)
{
  g_return_val_if_fail (IDE_IS_RUN_COMMAND (run_command), NULL);

  return g_object_new (IDE_TYPE_TEST,
                       "run-command", run_command,
                       NULL);
}

const char *
ide_test_get_id (IdeTest *self)
{
  g_return_val_if_fail (IDE_IS_TEST (self), NULL);
  g_return_val_if_fail (IDE_IS_RUN_COMMAND (self->run_command), NULL);

  return ide_run_command_get_id (self->run_command);
}

IdeTestStatus
ide_test_get_status (IdeTest *self)
{
  g_return_val_if_fail (IDE_IS_TEST (self), 0);

  return self->status;
}

const char *
ide_test_get_title (IdeTest *self)
{
  g_return_val_if_fail (IDE_IS_TEST (self), NULL);
  g_return_val_if_fail (IDE_IS_RUN_COMMAND (self->run_command), NULL);

  return ide_run_command_get_display_name (self->run_command);
}

const char *
ide_test_get_icon_name (IdeTest *self)
{
  g_return_val_if_fail (IDE_IS_TEST (self), NULL);

  switch (self->status)
    {
    case IDE_TEST_STATUS_NONE:
      return "builder-unit-tests-symbolic";

    case IDE_TEST_STATUS_RUNNING:
      return "builder-unit-tests-running-symbolic";

    case IDE_TEST_STATUS_FAILED:
      return "builder-unit-tests-fail-symbolic";

    case IDE_TEST_STATUS_SUCCESS:
      return "builder-unit-tests-pass-symbolic";

    default:
      g_return_val_if_reached (NULL);
    }
}

/**
 * ide_test_get_run_command:
 * @self: a #IdeTest
 *
 * Gets the run command for the test.
 *
 * Returns: (transfer none): an #IdeTest
 */
IdeRunCommand *
ide_test_get_run_command (IdeTest *self)
{
  g_return_val_if_fail (IDE_IS_TEST (self), NULL);

  return self->run_command;
}

static void
ide_test_wait_check_cb (GObject      *object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeTest *self;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);

  if (!ide_subprocess_wait_check_finish (subprocess, result, &error))
    {
      ide_test_set_status (self, IDE_TEST_STATUS_FAILED);
      ide_task_return_error (task, g_steal_pointer (&error));
    }
  else
    {
      ide_test_set_status (self, IDE_TEST_STATUS_SUCCESS);
      ide_task_return_boolean (task, TRUE);
    }

  IDE_EXIT;
}

void
ide_test_run_async (IdeTest             *self,
                    IdePipeline         *pipeline,
                    int                  pty_fd,
                    GCancellable        *cancellable,
                    GAsyncReadyCallback  callback,
                    gpointer             user_data)
{
  g_autoptr(IdeRunContext) run_context = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(IdeSettings) settings = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *locality = NULL;
  IdeContext *context;
  IdeRuntime *runtime;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_TEST (self));
  g_return_if_fail (IDE_IS_PIPELINE (pipeline));
  g_return_if_fail (IDE_IS_RUN_COMMAND (self->run_command));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_test_run_async);

  if (ide_task_return_error_if_cancelled (task))
    IDE_EXIT;

  context = ide_object_get_context (IDE_OBJECT (pipeline));
  runtime = ide_pipeline_get_runtime (pipeline);
  settings = ide_context_ref_settings (context, "org.gnome.builder.project");
  locality = ide_settings_get_string (settings, "unit-test-locality");

  if (ide_str_equal0 (locality, "runtime"))
    {
      run_context = ide_run_context_new ();
      ide_runtime_prepare_to_run (runtime, pipeline, run_context);
      ide_run_command_prepare_to_run (self->run_command, run_context, context);
    }
  else /* "pipeline" */
    {
      run_context = ide_pipeline_create_run_context (pipeline, self->run_command);
    }

  if (pty_fd > -1)
    {
      ide_run_context_take_fd (run_context, dup (pty_fd), STDIN_FILENO);
      ide_run_context_take_fd (run_context, dup (pty_fd), STDOUT_FILENO);
      ide_run_context_take_fd (run_context, dup (pty_fd), STDERR_FILENO);

      ide_run_context_setenv (run_context, "TERM", "xterm-256color");
    }

  if (!(subprocess = ide_run_context_spawn (run_context, &error)))
    {
      ide_test_set_status (self, IDE_TEST_STATUS_FAILED);
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }
  else
    {
      ide_subprocess_send_signal_upon_cancel (subprocess, cancellable, SIGKILL);
      ide_test_set_status (self, IDE_TEST_STATUS_RUNNING);
      ide_subprocess_wait_check_async (subprocess,
                                       cancellable,
                                       ide_test_wait_check_cb,
                                       g_steal_pointer (&task));
      IDE_EXIT;
    }
}

gboolean
ide_test_run_finish (IdeTest       *self,
                     GAsyncResult  *result,
                     GError       **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TEST (self));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}
