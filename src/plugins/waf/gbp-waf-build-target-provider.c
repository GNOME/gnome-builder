/* gbp-waf-build-target-provider.c
 *
 * Copyright 2019 Alex Mitchell
 * Copyright 2016-2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-waf-build-target-provider"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-foundry.h>
#include <libide-threading.h>

#include "gbp-waf-build-system.h"
#include "gbp-waf-build-target.h"
#include "gbp-waf-build-target-provider.h"

struct _GbpWafBuildTargetProvider
{
  IdeObject parent_instance;
};

static void
gbp_waf_build_target_provider_list_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GPtrArray) ar = NULL;
  g_autofree char *stdout_buf = NULL;
  IdeLineReader reader;
  char *line;
  gsize line_len;

  IDE_ENTRY;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_subprocess_communicate_utf8_finish (subprocess, result, &stdout_buf, NULL, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  ide_line_reader_init (&reader, stdout_buf, -1);

  /* Skip first two lines */
  ide_line_reader_next (&reader, &line_len);
  ide_line_reader_next (&reader, &line_len);

  ar = g_ptr_array_new_with_free_func (g_object_unref);

  while ((line = ide_line_reader_next (&reader, &line_len)))
    {
      line[line_len] = 0;

      g_strstrip (line);

      /* Skip last line -> "'list' finished successfully (time)" */
      if (g_str_has_prefix (line, "'list' "))
        break;

      g_ptr_array_add (ar, gbp_waf_build_target_new (line));
    }

  ide_task_return_pointer (task, g_steal_pointer (&ar), g_ptr_array_unref);

  IDE_EXIT;
}

static void
gbp_waf_build_target_provider_get_targets_async (IdeBuildTargetProvider *provider,
                                                 GCancellable           *cancellable,
                                                 GAsyncReadyCallback     callback,
                                                 gpointer                user_data)
{
  GbpWafBuildTargetProvider *self = (GbpWafBuildTargetProvider *)provider;
  g_autoptr(IdeRunContext) run_context = NULL;
  g_autoptr(IdeSubprocessLauncher) launcher  = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *waf = NULL;
  IdeBuildManager *build_manager;
  IdeBuildSystem *build_system;
  IdePipeline *pipeline;
  IdeContext *context;
  const char *python;

  IDE_ENTRY;

  g_assert (GBP_IS_WAF_BUILD_TARGET_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_waf_build_target_provider_get_targets_async);

  context = ide_object_get_context (IDE_OBJECT (self));
  build_system = ide_build_system_from_context (context);

  if (!GBP_IS_WAF_BUILD_SYSTEM (build_system))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Not a waf build system");
      IDE_EXIT;
    }

  build_manager = ide_build_manager_from_context (context);
  pipeline = ide_build_manager_get_pipeline (build_manager);

  if (pipeline == NULL ||
      !ide_pipeline_is_ready (pipeline) ||
      ide_pipeline_get_phase (pipeline) < IDE_PIPELINE_PHASE_CONFIGURE)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Pipeline not ready, cannot list run commands");
      IDE_EXIT;
    }

  waf = gbp_waf_build_system_locate_waf (GBP_WAF_BUILD_SYSTEM (build_system));
  if (gbp_waf_build_system_wants_python2 (GBP_WAF_BUILD_SYSTEM (build_system), NULL))
    python = "python2";
  else
    python = "python3";

  run_context = ide_run_context_new ();
  ide_pipeline_prepare_run_context (pipeline, run_context);
  ide_run_context_append_args (run_context, IDE_STRV_INIT (python, waf, "list", "--color=no"));

  if (!(launcher = ide_run_context_end (run_context, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  /* There appears to be some installations that will write to stderr instead of stdout */
  ide_subprocess_launcher_set_flags (launcher,
                                     (G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                      G_SUBPROCESS_FLAGS_STDERR_MERGE));
  ide_subprocess_launcher_set_cwd (launcher, ide_pipeline_get_srcdir (pipeline));

  if (!(subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  ide_subprocess_communicate_utf8_async (subprocess,
                                         NULL,
                                         cancellable,
                                         gbp_waf_build_target_provider_list_cb,
                                         g_steal_pointer (&task));

  IDE_EXIT;
}

static GPtrArray *
gbp_waf_build_target_provider_get_targets_finish (IdeBuildTargetProvider  *provider,
                                                  GAsyncResult            *result,
                                                  GError                 **error)
{
  GPtrArray *ret;

  IDE_ENTRY;

  g_assert (GBP_IS_WAF_BUILD_TARGET_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  /* transfer full semantics */
  IDE_PTR_ARRAY_CLEAR_FREE_FUNC (ret);

  IDE_RETURN (ret);
}

static void
build_target_provider_iface_init (IdeBuildTargetProviderInterface *iface)
{
  iface->get_targets_async = gbp_waf_build_target_provider_get_targets_async;
  iface->get_targets_finish = gbp_waf_build_target_provider_get_targets_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpWafBuildTargetProvider, gbp_waf_build_target_provider, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_TARGET_PROVIDER, build_target_provider_iface_init))

static void
gbp_waf_build_target_provider_class_init (GbpWafBuildTargetProviderClass *klass)
{
}

static void
gbp_waf_build_target_provider_init (GbpWafBuildTargetProvider *self)
{
}
