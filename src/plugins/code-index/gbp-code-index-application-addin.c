/* gbp-code-index-application-addin.c
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

#define G_LOG_DOMAIN "gbp-code-index-application-addin"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-code.h>
#include <libide-foundry.h>
#include <libide-gui.h>
#include <stdlib.h>

#include "gbp-code-index-application-addin.h"
#include "gbp-code-index-executor.h"
#include "gbp-code-index-plan.h"

struct _GbpCodeIndexApplicationAddin
{
  GObject parent_instance;
};

static void
gbp_code_index_application_addin_add_option_entries (IdeApplicationAddin *addin,
                                                     IdeApplication      *application)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_APPLICATION_ADDIN (addin));
  g_assert (IDE_IS_APPLICATION (application));

  g_application_add_main_option (G_APPLICATION (application),
                                 "index",
                                 'i',
                                 G_OPTION_FLAG_IN_MAIN,
                                 G_OPTION_ARG_FILENAME,
                                 _("Create or update code-index for project file"),
                                 _("PROJECT_FILE"));
}

static const gchar *
reason_string (GbpCodeIndexReason reason)
{
  switch (reason)
    {
    case GBP_CODE_INDEX_REASON_INITIAL: return "initial";
    case GBP_CODE_INDEX_REASON_CHANGES: return "changes";
    case GBP_CODE_INDEX_REASON_REMOVE_INDEX: return "remove-index";
    case GBP_CODE_INDEX_REASON_EXPIRED: return "expired";
    default: return "unknown";
    }
}

static gboolean
gbp_code_index_application_addin_foreach_cb (GFile              *directory,
                                             GPtrArray          *plan_items,
                                             GbpCodeIndexReason  reason,
                                             gpointer            user_data)
{
  GApplicationCommandLine *cmdline = user_data;

  g_assert (G_IS_FILE (directory));
  g_assert (plan_items != NULL);
  g_assert (G_IS_APPLICATION_COMMAND_LINE (cmdline));

  g_application_command_line_print (cmdline,
                                    "%s [reason=%s]\n",
                                    g_file_peek_path (directory),
                                    reason_string (reason));

  for (guint i = 0; i < plan_items->len; i++)
    {
      const GbpCodeIndexPlanItem *item = g_ptr_array_index (plan_items, i);
      const gchar *name = g_file_info_get_name (item->file_info);
      g_autofree gchar *flags = NULL;

      if (item->build_flags)
        flags = g_strjoinv ("' '", item->build_flags);

      g_application_command_line_print (cmdline,
                                        "  %s [indexer=%s] -- '%s'\n",
                                        name,
                                        item->indexer_module_name,
                                        flags ? flags : "");
    }

  return FALSE;
}

static void
gbp_code_index_application_addin_execute_cb (GObject      *object,
                                             GAsyncResult *result,
                                             gpointer      user_data)
{
  GbpCodeIndexExecutor *executor = (GbpCodeIndexExecutor *)object;
  g_autoptr(GApplicationCommandLine) cmdline = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (G_IS_APPLICATION_COMMAND_LINE (cmdline));

  if (!gbp_code_index_executor_execute_finish (executor, result, &error))
    {
      g_application_command_line_printerr (cmdline, "Indexing failed: %s\n", error->message);
      g_application_command_line_set_exit_status (cmdline, EXIT_FAILURE);
    }
  else
    {
      g_application_command_line_print (cmdline, "Indexing complete.\n");
      g_application_command_line_set_exit_status (cmdline, EXIT_SUCCESS);
    }

  ide_object_destroy (IDE_OBJECT (executor));
}

static void
gbp_code_index_application_addin_load_flags_cb (GObject      *object,
                                                GAsyncResult *result,
                                                gpointer      user_data)
{
  GbpCodeIndexPlan *plan = (GbpCodeIndexPlan *)object;
  g_autoptr(GApplicationCommandLine) cmdline = user_data;
  g_autoptr(GbpCodeIndexExecutor) executor = NULL;
  g_autoptr(GError) error = NULL;
  IdeWorkbench *workbench;
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_PLAN (plan));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_APPLICATION_COMMAND_LINE (cmdline));

  if (!gbp_code_index_plan_load_flags_finish (plan, result, &error))
    {
      g_application_command_line_printerr (cmdline,
                                           _("Failed to load flags for plan: %s"),
                                           error->message);
      g_application_command_line_set_exit_status (cmdline, EXIT_FAILURE);
      return;
    }

  if (FALSE)
  gbp_code_index_plan_foreach (plan,
                               gbp_code_index_application_addin_foreach_cb,
                               cmdline);

  workbench = g_object_get_data (G_OBJECT (cmdline), "WORKBENCH");
  context = ide_workbench_get_context (workbench);

  executor = gbp_code_index_executor_new (plan);
  ide_object_append (IDE_OBJECT (context), IDE_OBJECT (executor));

  gbp_code_index_executor_execute_async (executor,
                                         NULL,
                                         NULL,
                                         gbp_code_index_application_addin_execute_cb,
                                         g_steal_pointer (&cmdline));
}

static void
gbp_code_index_application_addin_cull_cb (GObject      *object,
                                          GAsyncResult *result,
                                          gpointer      user_data)
{
  GbpCodeIndexPlan *plan = (GbpCodeIndexPlan *)object;
  g_autoptr(GApplicationCommandLine) cmdline = user_data;
  g_autoptr(GError) error = NULL;
  IdeWorkbench *workbench;
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_PLAN (plan));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_APPLICATION_COMMAND_LINE (cmdline));

  if (!gbp_code_index_plan_cull_indexed_finish (plan, result, &error))
    {
      g_application_command_line_printerr (cmdline,
                                           _("Failed to cull index plan: %s"),
                                           error->message);
      g_application_command_line_set_exit_status (cmdline, EXIT_FAILURE);
      return;
    }

  workbench = g_object_get_data (G_OBJECT (cmdline), "WORKBENCH");
  context = ide_workbench_get_context (workbench);

  gbp_code_index_plan_load_flags_async (plan,
                                        context,
                                        NULL,
                                        gbp_code_index_application_addin_load_flags_cb,
                                        g_steal_pointer (&cmdline));
}

static void
gbp_code_index_application_addin_populate_cb (GObject      *object,
                                              GAsyncResult *result,
                                              gpointer      user_data)
{
  GbpCodeIndexPlan *plan = (GbpCodeIndexPlan *)object;
  g_autoptr(GApplicationCommandLine) cmdline = user_data;
  g_autoptr(GError) error = NULL;
  IdeWorkbench *workbench;
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_PLAN (plan));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_APPLICATION_COMMAND_LINE (cmdline));

  if (!gbp_code_index_plan_populate_finish (plan, result, &error))
    {
      g_application_command_line_printerr (cmdline,
                                           _("Failed to populate index plan: %s"),
                                           error->message);
      g_application_command_line_set_exit_status (cmdline, EXIT_FAILURE);
      return;
    }

  workbench = g_object_get_data (G_OBJECT (cmdline), "WORKBENCH");
  context = ide_workbench_get_context (workbench);

  gbp_code_index_plan_cull_indexed_async (plan,
                                          context,
                                          NULL,
                                          gbp_code_index_application_addin_cull_cb,
                                          g_steal_pointer (&cmdline));
}

static void
gbp_code_index_application_addin_load_project_cb (GObject      *object,
                                                  GAsyncResult *result,
                                                  gpointer      user_data)
{
  IdeWorkbench *workbench = (IdeWorkbench *)object;
  g_autoptr(GApplicationCommandLine) cmdline = user_data;
  g_autoptr(GbpCodeIndexPlan) plan = NULL;
  g_autoptr(GError) error = NULL;
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_WORKBENCH (workbench));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_APPLICATION_COMMAND_LINE (cmdline));

  if (!ide_workbench_load_project_finish (workbench, result, &error))
    {
      g_application_command_line_printerr (cmdline,
                                           _("Failed to load project: %s"),
                                           error->message);
      g_application_command_line_set_exit_status (cmdline, EXIT_FAILURE);
      return;
    }

  context = ide_workbench_get_context (workbench);
  plan = gbp_code_index_plan_new ();

  gbp_code_index_plan_populate_async (plan,
                                      context,
                                      NULL,
                                      gbp_code_index_application_addin_populate_cb,
                                      g_steal_pointer (&cmdline));
}

static void
gbp_code_index_application_addin_handle_command_line (IdeApplicationAddin     *addin,
                                                      IdeApplication          *application,
                                                      GApplicationCommandLine *cmdline)
{
  g_autofree gchar *project_path = NULL;
  g_autoptr(IdeWorkbench) workbench = NULL;
  g_autoptr(IdeProjectInfo) project_info = NULL;
  g_autoptr(GFile) project_file = NULL;
  g_autoptr(GFile) project_dir = NULL;
  GVariantDict *options;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_APPLICATION_ADDIN (addin));
  g_assert (IDE_IS_APPLICATION (application));
  g_assert (G_IS_APPLICATION_COMMAND_LINE (cmdline));

  if (!(options = g_application_command_line_get_options_dict (cmdline)) ||
      !g_variant_dict_contains (options, "index") ||
      !g_variant_dict_lookup (options, "index", "^ay", &project_path))
    return;

  project_file = g_file_new_for_path (project_path);

  if (g_file_test (project_path, G_FILE_TEST_IS_DIR))
    project_dir = g_object_ref (project_file);
  else
    project_file = g_file_get_parent (project_file);

  project_info = ide_project_info_new ();
  ide_project_info_set_file (project_info, project_file);
  ide_project_info_set_directory (project_info, project_dir);

  workbench = ide_workbench_new ();
  ide_application_add_workbench (application, workbench);
  ide_workbench_load_project_async (workbench,
                                    project_info,
                                    G_TYPE_INVALID,
                                    NULL,
                                    gbp_code_index_application_addin_load_project_cb,
                                    g_object_ref (cmdline));

  ide_application_set_command_line_handled (application, cmdline, TRUE);
  g_object_set_data_full (G_OBJECT (cmdline), "WORKBENCH", g_steal_pointer (&workbench), g_object_unref);
  g_object_set_data_full (G_OBJECT (cmdline), "APP", application, (GDestroyNotify)g_application_release);
  g_application_hold (G_APPLICATION (application));
}

static void
application_addin_iface_init (IdeApplicationAddinInterface *iface)
{
  iface->add_option_entries = gbp_code_index_application_addin_add_option_entries;
  iface->handle_command_line = gbp_code_index_application_addin_handle_command_line;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpCodeIndexApplicationAddin, gbp_code_index_application_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_APPLICATION_ADDIN, application_addin_iface_init))

static void
gbp_code_index_application_addin_class_init (GbpCodeIndexApplicationAddinClass *klass)
{
}

static void
gbp_code_index_application_addin_init (GbpCodeIndexApplicationAddin *self)
{
}
