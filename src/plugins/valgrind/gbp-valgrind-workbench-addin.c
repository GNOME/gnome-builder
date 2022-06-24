/* gbp-valgrind-workbench-addin.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-valgrind-workbench-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <errno.h>
#include <unistd.h>

#include <libide-foundry.h>
#include <libide-gui.h>

#include "gbp-valgrind-workbench-addin.h"

struct _GbpValgrindWorkbenchAddin
{
  GObject             parent_instance;

  IdeWorkbench       *workbench;
  IdeRunManager      *run_manager;
  IdeBuildManager    *build_manager;
  char               *log_name;
  GSimpleActionGroup *actions;

  gulong              notify_pipeline_handler;
  gulong              stopped_handler;

  guint               has_handler : 1;
};

static void
set_state (GSimpleAction *action,
           GVariant      *param,
           gpointer       user_data)
{
  g_simple_action_set_state (action, param);
}

static gboolean
get_bool (GbpValgrindWorkbenchAddin *self,
          const char                *action_name)
{
  g_autoptr(GVariant) state = NULL;
  GAction *action;

  g_assert (GBP_IS_VALGRIND_WORKBENCH_ADDIN (self));
  g_assert (action_name != NULL);

  if (!(action = g_action_map_lookup_action (G_ACTION_MAP (self->actions), action_name)))
    g_return_val_if_reached (FALSE);

  if (!(state = g_action_get_state (action)))
    g_return_val_if_reached (FALSE);

  if (!g_variant_is_of_type (state, G_VARIANT_TYPE_BOOLEAN))
    g_return_val_if_reached (FALSE);

  return g_variant_get_boolean (state);
}

static const char *
get_string (GbpValgrindWorkbenchAddin *self,
            const char                *action_name)
{
  g_autoptr(GVariant) state = NULL;
  GAction *action;

  g_assert (GBP_IS_VALGRIND_WORKBENCH_ADDIN (self));
  g_assert (action_name != NULL);

  if (!(action = g_action_map_lookup_action (G_ACTION_MAP (self->actions), action_name)))
    g_return_val_if_reached (NULL);

  if (!(state = g_action_get_state (action)))
    g_return_val_if_reached (NULL);

  if (!g_variant_is_of_type (state, G_VARIANT_TYPE_STRING))
    g_return_val_if_reached (NULL);

  return g_variant_get_string (state, NULL);
}

static void
gbp_valgrind_workbench_addin_stop_cb (GbpValgrindWorkbenchAddin *self,
                                      IdeRunManager             *run_manager)
{
  g_autoptr(GFile) file = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_VALGRIND_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_RUN_MANAGER (run_manager));

  if (self->workbench == NULL || self->log_name == NULL)
    IDE_EXIT;

  file = g_file_new_for_path (self->log_name);
  ide_workbench_open_async (self->workbench,
                            file,
                            "editorui",
                            IDE_BUFFER_OPEN_FLAGS_NONE,
                            NULL, NULL, NULL, NULL);
  g_clear_pointer (&self->log_name, g_free);

  IDE_EXIT;
}

static gboolean
gbp_valgrind_workbench_addin_run_handler_cb (IdeRunContext       *run_context,
                                             const char * const  *argv,
                                             const char * const  *env,
                                             const char          *cwd,
                                             IdeUnixFDMap        *unix_fd_map,
                                             gpointer             user_data,
                                             GError             **error)
{
  GbpValgrindWorkbenchAddin *self = user_data;
  g_autofree char *name = NULL;
  g_autofree char *track_origins = NULL;
  g_autofree char *leak_check = NULL;
  g_autoptr(GString) leak_kinds = NULL;
  int source_fd;
  int dest_fd;

  IDE_ENTRY;

  g_assert (IDE_IS_RUN_CONTEXT (run_context));
  g_assert (IDE_IS_UNIX_FD_MAP (unix_fd_map));
  g_assert (argv != NULL);
  g_assert (env != NULL);
  g_assert (GBP_IS_VALGRIND_WORKBENCH_ADDIN (self));

  if (cwd != NULL)
    ide_run_context_set_cwd (run_context, cwd);

  dest_fd = ide_unix_fd_map_get_max_dest_fd (unix_fd_map) + 1;
  if (!ide_run_context_merge_unix_fd_map (run_context, unix_fd_map, error))
    IDE_RETURN (FALSE);

  /* Create a temp file to write to and an FD to access it */
  if (-1 == (source_fd = g_file_open_tmp ("gnome-builder-valgrind-XXXXXX.txt", &name, error)))
    IDE_RETURN (FALSE);

  /* Set our FD for valgrind to log to */
  ide_run_context_take_fd (run_context, source_fd, dest_fd);

  /* Save the filename so we can open it after exiting */
  g_clear_pointer (&self->log_name, g_free);
  self->log_name = g_steal_pointer (&name);

  /* Convert action state to command-line arguments */
  track_origins = g_strdup_printf ("--track-origins=%s", get_bool (self, "track-origins") ? "yes" : "no");
  leak_check = g_strdup_printf ("--leak-check=%s", get_string (self, "leak-check"));

  leak_kinds = g_string_new (NULL);
  if (get_bool (self, "leak-kind-definite")) g_string_append (leak_kinds, "definite,");
  if (get_bool (self, "leak-kind-possible")) g_string_append (leak_kinds, "possible,");
  if (get_bool (self, "leak-kind-indirect")) g_string_append (leak_kinds, "indirect,");
  if (get_bool (self, "leak-kind-reachable")) g_string_append (leak_kinds, "reachable,");

  ide_run_context_append_argv (run_context, "valgrind");
  ide_run_context_append_formatted (run_context, "--log-fd=%d", dest_fd);
  ide_run_context_append_argv (run_context, leak_check);
  ide_run_context_append_argv (run_context, track_origins);

  if (leak_kinds->len > 0)
    {
      g_string_truncate (leak_kinds, leak_kinds->len-1);
      ide_run_context_append_formatted (run_context, "--show-leak-kinds=%s", leak_kinds->str);
    }

  if (env[0] != NULL)
    {
      /* If we have to exec "env" to pass environment variables, then we
       * must follow children to get to our target executable.
       */
      ide_run_context_append_argv (run_context, "--trace-children=yes");
      ide_run_context_append_argv (run_context, "env");
      ide_run_context_append_args (run_context, env);
    }

  ide_run_context_append_args (run_context, argv);

  IDE_RETURN (TRUE);
}

static void
gbp_valgrind_workbench_addin_run_handler (IdeRunManager *run_manager,
                                          IdePipeline   *pipeline,
                                          IdeRunCommand *run_command,
                                          IdeRunContext *run_context,
                                          gpointer       user_data)
{
  GbpValgrindWorkbenchAddin *self = user_data;

  IDE_ENTRY;

  g_assert (IDE_IS_RUN_MANAGER (run_manager));
  g_assert (IDE_IS_RUN_CONTEXT (run_context));
  g_assert (GBP_IS_VALGRIND_WORKBENCH_ADDIN (self));

  ide_run_context_push (run_context,
                        gbp_valgrind_workbench_addin_run_handler_cb,
                        g_object_ref (self),
                        g_object_unref);

  IDE_EXIT;
}

static void
gbp_valgrind_workbench_addin_notify_pipeline_cb (GbpValgrindWorkbenchAddin *self,
                                                 GParamSpec                *pspec,
                                                 IdeBuildManager           *build_manager)
{
  IdePipeline *pipeline;
  IdeRuntime *runtime;
  gboolean can_handle = FALSE;

  IDE_ENTRY;

  g_assert (GBP_IS_VALGRIND_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));
  g_assert (IDE_IS_RUN_MANAGER (self->run_manager));

  if (!(pipeline = ide_build_manager_get_pipeline (build_manager)) ||
      !(runtime = ide_pipeline_get_runtime (pipeline)) ||
      !ide_runtime_contains_program_in_path (runtime, "valgrind", NULL))
    IDE_GOTO (not_found);

  can_handle = TRUE;

not_found:
  if (can_handle != self->has_handler)
    {
      self->has_handler = can_handle;

      if (can_handle)
        ide_run_manager_add_handler (self->run_manager,
                                     "valgrind",
                                     _("Run with Valgrind"),
                                     "system-run-symbolic",
                                     gbp_valgrind_workbench_addin_run_handler,
                                     g_object_ref (self),
                                     g_object_unref);
      else
        ide_run_manager_remove_handler (self->run_manager, "valgrind");
    }

  IDE_EXIT;
}

static void
gbp_valgrind_workbench_addin_project_loaded (IdeWorkbenchAddin *addin,
                                             IdeProjectInfo    *project_info)
{
  GbpValgrindWorkbenchAddin *self = (GbpValgrindWorkbenchAddin *)addin;
  IdeBuildManager *build_manager;
  IdeRunManager *run_manager;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (GBP_IS_VALGRIND_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_PROJECT_INFO (project_info));
  g_assert (IDE_IS_WORKBENCH (self->workbench));

  context = ide_workbench_get_context (self->workbench);
  build_manager = ide_build_manager_from_context (context);
  run_manager = ide_run_manager_from_context (context);

  g_set_object (&self->build_manager, build_manager);
  g_set_object (&self->run_manager, run_manager);

  self->notify_pipeline_handler =
    g_signal_connect_object (build_manager,
                             "notify::pipeline",
                             G_CALLBACK (gbp_valgrind_workbench_addin_notify_pipeline_cb),
                             self,
                             G_CONNECT_SWAPPED);

  self->stopped_handler =
    g_signal_connect_object (run_manager,
                             "stopped",
                             G_CALLBACK (gbp_valgrind_workbench_addin_stop_cb),
                             self,
                             G_CONNECT_SWAPPED);

  gbp_valgrind_workbench_addin_notify_pipeline_cb (self, NULL, build_manager);

  IDE_EXIT;
}

static const GActionEntry actions[] = {
  { "track-origins", NULL, NULL, "true", set_state },
  { "leak-check", NULL, "s", "'summary'", set_state },
  { "leak-kind-definite", NULL, NULL, "true", set_state },
  { "leak-kind-possible", NULL, NULL, "true", set_state },
  { "leak-kind-indirect", NULL, NULL, "false", set_state },
  { "leak-kind-reachable", NULL, NULL, "false", set_state },
};

static void
gbp_valgrind_workbench_addin_load (IdeWorkbenchAddin *addin,
                                   IdeWorkbench      *workbench)
{
  GbpValgrindWorkbenchAddin *self = (GbpValgrindWorkbenchAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_VALGRIND_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = workbench;

  self->actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (self->actions),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);

  IDE_EXIT;
}

static void
gbp_valgrind_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                     IdeWorkbench      *workbench)
{
  GbpValgrindWorkbenchAddin *self = (GbpValgrindWorkbenchAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_VALGRIND_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  if (self->build_manager != NULL)
    {
      g_clear_signal_handler (&self->notify_pipeline_handler, self->build_manager);
      g_clear_object (&self->build_manager);
    }

  if (self->run_manager != NULL)
    {
      g_clear_signal_handler (&self->stopped_handler, self->run_manager);

      if (self->has_handler)
        ide_run_manager_remove_handler (self->run_manager, "valgrind");

      g_clear_object (&self->run_manager);
    }

  g_clear_pointer (&self->log_name, g_free);

  g_clear_object (&self->actions);

  self->workbench = NULL;

  IDE_EXIT;
}

static void
gbp_valgrind_workbench_addin_workspace_added (IdeWorkbenchAddin *addin,
                                              IdeWorkspace      *workspace)
{
  GbpValgrindWorkbenchAddin *self = (GbpValgrindWorkbenchAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_VALGRIND_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  gtk_widget_insert_action_group (GTK_WIDGET (workspace),
                                  "valgrind",
                                  G_ACTION_GROUP (self->actions));

  IDE_EXIT;
}

static void
gbp_valgrind_workbench_addin_workspace_removed (IdeWorkbenchAddin *addin,
                                                IdeWorkspace      *workspace)
{
  GbpValgrindWorkbenchAddin *self = (GbpValgrindWorkbenchAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_VALGRIND_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  gtk_widget_insert_action_group (GTK_WIDGET (workspace), "valgrind", NULL);

  IDE_EXIT;
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = gbp_valgrind_workbench_addin_load;
  iface->unload = gbp_valgrind_workbench_addin_unload;
  iface->project_loaded = gbp_valgrind_workbench_addin_project_loaded;
  iface->workspace_added = gbp_valgrind_workbench_addin_workspace_added;
  iface->workspace_removed = gbp_valgrind_workbench_addin_workspace_removed;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpValgrindWorkbenchAddin, gbp_valgrind_workbench_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN, workbench_addin_iface_init))

static void
gbp_valgrind_workbench_addin_class_init (GbpValgrindWorkbenchAddinClass *klass)
{
}

static void
gbp_valgrind_workbench_addin_init (GbpValgrindWorkbenchAddin *self)
{
}
