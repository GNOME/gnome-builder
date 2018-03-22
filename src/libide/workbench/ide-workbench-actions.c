/* ide-workbench-actions.c
 *
 * Copyright © 2015 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-workbench"

#include "config.h"

#include <dazzle.h>
#include <glib/gi18n.h>
#include <libpeas/peas.h>
#include <libpeas/peas-autocleanups.h>
#include <unistd.h>

#include "ide-debug.h"

#include "application/ide-application.h"
#include "buffers/ide-buffer-manager.h"
#include "buildsystem/ide-dependency-updater.h"
#include "vcs/ide-vcs.h"
#include "workbench/ide-workbench.h"
#include "workbench/ide-workbench-header-bar.h"
#include "workbench/ide-workbench-private.h"

static void
ide_workbench_actions_open_with_dialog_cb (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  IdeWorkbench *self = (IdeWorkbench *)object;
  GError *error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_WORKBENCH (self));

  if (!ide_workbench_open_files_finish (self, result, &error))
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
    }

  IDE_EXIT;
}

static void
ide_workbench_actions_open_with_dialog (GSimpleAction *action,
                                        GVariant      *param,
                                        gpointer       user_data)
{
  IdeWorkbench *self = user_data;
  GtkFileChooserNative *native;
  IdeContext *context;
  IdeVcs *vcs;
  GFile *workdir;
  gint ret;

  IDE_ENTRY;

  g_assert (IDE_IS_WORKBENCH (self));

  context = ide_workbench_get_context (self);
  vcs = ide_context_get_vcs (context);
  workdir = ide_vcs_get_working_directory (vcs);

  native = gtk_file_chooser_native_new (_("Open File"),
                                        GTK_WINDOW (self),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        _("Open"),
                                        _("Cancel"));
  gtk_file_chooser_set_current_folder_file (GTK_FILE_CHOOSER (native), workdir, NULL);
  gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (native), FALSE);

  /* Unlike gtk_dialog_run(), this will handle processing
   * various I/O events and so should be safe to use.
   */
  ret = gtk_native_dialog_run (GTK_NATIVE_DIALOG (native));

  if (ret == GTK_RESPONSE_ACCEPT)
    {
      g_autoptr(GFile) file = NULL;

      IDE_PROBE;

      file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (native));
      ide_workbench_open_files_async (self,
                                      &file,
                                      1,
                                      NULL,
                                      IDE_WORKBENCH_OPEN_FLAGS_NONE,
                                      NULL,
                                      ide_workbench_actions_open_with_dialog_cb,
                                      NULL);
    }

  gtk_native_dialog_hide (GTK_NATIVE_DIALOG (native));
  gtk_native_dialog_destroy (GTK_NATIVE_DIALOG (native));

  IDE_EXIT;
}

static void
ide_workbench_actions_save_all (GSimpleAction *action,
                                GVariant      *variant,
                                gpointer       user_data)
{
  IdeWorkbench *workbench = user_data;
  IdeContext *context;
  IdeBufferManager *bufmgr;

  g_assert (IDE_IS_WORKBENCH (workbench));

  context = ide_workbench_get_context (workbench);
  if (context == NULL)
    return;

  bufmgr = ide_context_get_buffer_manager (context);
  ide_buffer_manager_save_all_async (bufmgr, NULL, NULL, NULL);
}

static void
save_all_quit_cb (GObject      *object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  IdeBufferManager *bufmgr = (IdeBufferManager *)object;
  g_autoptr(IdeWorkbench) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_BUFFER_MANAGER (bufmgr));
  g_assert (IDE_IS_WORKBENCH (self));

  if (!ide_buffer_manager_save_all_finish (bufmgr, result, &error))
    {
      g_warning ("%s", error->message);
      return;
    }

  g_application_quit (G_APPLICATION (IDE_APPLICATION_DEFAULT));
}

static void
ide_workbench_actions_save_all_quit (GSimpleAction *action,
                                     GVariant      *variant,
                                     gpointer       user_data)
{
  IdeWorkbench *workbench = user_data;
  IdeContext *context;
  IdeBufferManager *bufmgr;

  g_assert (IDE_IS_WORKBENCH (workbench));

  context = ide_workbench_get_context (workbench);
  if (context == NULL)
    return;

  bufmgr = ide_context_get_buffer_manager (context);
  ide_buffer_manager_save_all_async (bufmgr,
                                     NULL,
                                     save_all_quit_cb,
                                     g_object_ref (workbench));
}

static void
ide_workbench_actions_opacity (GSimpleAction *action,
                               GVariant      *variant,
                               gpointer       user_data)
{
  IdeWorkbench *workbench = user_data;
  gdouble opacity;

  g_assert (IDE_IS_WORKBENCH (workbench));
  g_assert (g_variant_is_of_type (variant, G_VARIANT_TYPE_INT32));

  opacity = CLAMP (g_variant_get_int32 (variant), 10, 100) / 100.0;
  gtk_widget_set_opacity (GTK_WIDGET (workbench), opacity);
}

static void
ide_workbench_actions_global_search (GSimpleAction *action,
                                     GVariant      *variant,
                                     gpointer       user_data)
{
  IdeWorkbench *self = user_data;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_WORKBENCH (self));

  ide_workbench_header_bar_focus_search (self->header_bar);
}

static void
ide_workbench_actions_counters (GSimpleAction *action,
                                GVariant      *variant,
                                gpointer       user_data)
{
  IdeWorkbench *self = user_data;
  DzlCounterArena *arena;
  GtkWindow *window;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_WORKBENCH (self));

  arena = dzl_counter_arena_new_for_pid (getpid ());
  window = g_object_new (DZL_TYPE_COUNTERS_WINDOW,
                         "title", _("Builder Statistics"),
                         "default-width", 800,
                         "default-height", 600,
                         "transient-for", self,
                         "modal", FALSE,
                         NULL);
  dzl_counters_window_set_arena (DZL_COUNTERS_WINDOW (window), arena);
  gtk_window_present (window);

  dzl_counter_arena_unref (arena);
}

static void
ide_workbench_actions_inspector (GSimpleAction *action,
                                 GVariant      *variant,
                                 gpointer       user_data)
{
  gtk_window_set_interactive_debugging (TRUE);
}

static void
ide_workbench_actions_update_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  IdeDependencyUpdater *updater = (IdeDependencyUpdater *)object;
  g_autoptr(IdeWorkbench) self = user_data;
  g_autoptr(GError) error = NULL;
  IdeContext *context;

  g_assert (IDE_IS_DEPENDENCY_UPDATER (updater));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_WORKBENCH (self));

  context = ide_workbench_get_context (self);

  if (!ide_dependency_updater_update_finish (updater, result, &error))
    ide_context_warning (context, "%s", error->message);
}

static void
ide_workbench_actions_update_dependencies_cb (PeasExtensionSet *set,
                                              PeasPluginInfo   *plugin_info,
                                              PeasExtension    *exten,
                                              gpointer          user_data)
{
  IdeDependencyUpdater *updater = (IdeDependencyUpdater *)exten;
  IdeWorkbench *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_DEPENDENCY_UPDATER (updater));
  g_assert (IDE_IS_WORKBENCH (self));

  ide_dependency_updater_update_async (updater,
                                       NULL,
                                       ide_workbench_actions_update_cb,
                                       g_object_ref (self));
}

static void
ide_workbench_actions_update_dependencies (GSimpleAction *action,
                                           GVariant      *variant,
                                           gpointer       user_data)
{
  g_autoptr(PeasExtensionSet) set = NULL;
  IdeWorkbench *self = user_data;
  IdeContext *context;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_WORKBENCH (self));

  context = ide_workbench_get_context (self);
  if (context == NULL)
    return;

  set = peas_extension_set_new (peas_engine_get_default (),
                                IDE_TYPE_DEPENDENCY_UPDATER,
                                "context", context,
                                NULL);
  peas_extension_set_foreach (set,
                              ide_workbench_actions_update_dependencies_cb,
                              self);
}

void
ide_workbench_actions_init (IdeWorkbench *self)
{
  GPropertyAction *action;
  const GActionEntry actions[] = {
    { "global-search", ide_workbench_actions_global_search },
    { "opacity", NULL, "i", "100", ide_workbench_actions_opacity },
    { "open-with-dialog", ide_workbench_actions_open_with_dialog },
    { "save-all", ide_workbench_actions_save_all },
    { "save-all-quit", ide_workbench_actions_save_all_quit },
    { "counters", ide_workbench_actions_counters },
    { "inspector", ide_workbench_actions_inspector },
    { "update-dependencies", ide_workbench_actions_update_dependencies },
  };

  g_action_map_add_action_entries (G_ACTION_MAP (self), actions, G_N_ELEMENTS (actions), self);

  action = g_property_action_new ("perspective", self, "visible-perspective-name");
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (action));
  g_object_unref (action);
}
