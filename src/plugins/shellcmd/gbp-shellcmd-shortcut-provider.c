/* gbp-shellcmd-shortcut-provider.c
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

#define G_LOG_DOMAIN "gbp-shellcmd-shortcut-provider"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-editor.h>
#include <libide-gui.h>
#include <libide-terminal.h>

#include "gbp-shellcmd-command-model.h"
#include "gbp-shellcmd-run-command.h"
#include "gbp-shellcmd-shortcut-provider.h"

struct _GbpShellcmdShortcutProvider
{
  IdeObject   parent_instance;
  GListStore *model;
};

static gboolean
gbp_shellcmd_shortcut_func (GtkWidget *widget,
                            GVariant  *args,
                            gpointer   user_data)
{
  GbpShellcmdRunCommand *run_command = user_data;
  g_autoptr(PanelPosition) position = NULL;
  g_autoptr(IdeTerminalLauncher) launcher = NULL;
  g_auto(GStrv) override_environ = NULL;
  IdeWorkspace *workspace;
  IdeContext *context;
  const char *title;
  IdePage *current_page;
  IdePage *page;

  IDE_ENTRY;

  g_assert (GTK_IS_WIDGET (widget));
  g_assert (args == NULL);
  g_assert (GBP_IS_SHELLCMD_RUN_COMMAND (run_command));

  g_debug ("Shortcut triggered to run command “%s” which has accelerator %s",
           ide_run_command_get_display_name (IDE_RUN_COMMAND (run_command)),
           gbp_shellcmd_run_command_get_accelerator (run_command));

  if (!(workspace = ide_widget_get_workspace (widget)) ||
      !(context = ide_workspace_get_context (workspace)))
    IDE_RETURN (FALSE);

  if (!IDE_IS_PRIMARY_WORKSPACE (workspace) &&
      !IDE_IS_EDITOR_WORKSPACE (workspace))
    IDE_RETURN (FALSE);

  if (!(title = ide_run_command_get_display_name (IDE_RUN_COMMAND (run_command))))
    title = _("Untitled command");

  launcher = ide_terminal_launcher_new (context, IDE_RUN_COMMAND (run_command));

  if ((current_page = ide_workspace_get_most_recent_page (workspace)) &&
      IDE_IS_EDITOR_PAGE (current_page))
    {
      GFile *file = ide_editor_page_get_file (IDE_EDITOR_PAGE (current_page));

      if (file != NULL)
        {
          g_autofree char *uri = g_file_get_uri (file);

          if (uri != NULL)
            override_environ = g_environ_setenv (override_environ,
                                                 "CURRENT_FILE_URI",
                                                 uri,
                                                 FALSE);

          if (g_file_is_native (file))
            override_environ = g_environ_setenv (override_environ,
                                                 "CURRENT_FILE_PATH",
                                                 g_file_peek_path (file),
                                                 FALSE);
        }
    }

  ide_terminal_launcher_set_override_environ (launcher,
                                              (const char * const *)override_environ);

  page = g_object_new (IDE_TYPE_TERMINAL_PAGE,
                       "close-on-exit", FALSE,
                       "icon-name", "text-x-script-symbolic",
                       "launcher", launcher,
                       "manage-spawn", TRUE,
                       "respawn-on-exit", FALSE,
                       "title", title,
                       NULL);

  position = panel_position_new ();

  ide_workspace_add_page (workspace, page, position);
  panel_widget_raise (PANEL_WIDGET (page));
  gtk_widget_grab_focus (GTK_WIDGET (page));

  IDE_RETURN (TRUE);
}

static gboolean
accelerator_to_trigger (GBinding     *binding,
                        const GValue *from_value,
                        GValue       *to_value,
                        gpointer      user_data)
{
  const char *accel;

  g_assert (G_IS_BINDING (binding));
  g_assert (G_VALUE_HOLDS_STRING (from_value));

  accel = g_value_get_string (from_value);

  if (!ide_str_empty0 (accel))
    {
      GtkShortcutTrigger *trigger;

      if ((trigger = gtk_shortcut_trigger_parse_string (accel)))
        {
          g_value_take_object (to_value, trigger);
          return TRUE;
        }
    }

  g_value_set_object (to_value, gtk_never_trigger_get ());

  return TRUE;
}

static gpointer
gbp_shellcmd_shortcut_provider_map_func (gpointer item,
                                         gpointer user_data)
{
  g_autoptr(GbpShellcmdRunCommand) command = item;
  g_autoptr(GtkShortcutAction) action = NULL;
  GtkShortcut *ret;

  g_assert (GBP_IS_SHELLCMD_RUN_COMMAND (command));
  g_assert (user_data == NULL);

  action = gtk_callback_action_new (gbp_shellcmd_shortcut_func,
                                    g_object_ref (command),
                                    g_object_unref);
  ret = gtk_shortcut_new (NULL, g_steal_pointer (&action));

  /* We want the accelerator to update when the command changes it */
  g_object_bind_property_full (command, "accelerator", ret, "trigger",
                               G_BINDING_SYNC_CREATE,
                               accelerator_to_trigger, NULL,
                               NULL, NULL);

  /* Our capture/bubble filters require a phase set. Keep in sync
   * with ide_shortcut_is_phase().
   */
  g_object_set_data (G_OBJECT (ret),
                     "PHASE",
                     GINT_TO_POINTER (GTK_PHASE_BUBBLE));

  return g_steal_pointer (&ret);
}

static void
add_with_mapping (GListStore *store,
                  GListModel *commands,
                  gboolean    prepend)
{
  g_autoptr(GtkMapListModel) map = NULL;

  g_assert (G_IS_LIST_STORE (store));
  g_assert (GBP_IS_SHELLCMD_COMMAND_MODEL (commands));

  map = gtk_map_list_model_new (g_object_ref (commands),
                                gbp_shellcmd_shortcut_provider_map_func,
                                NULL, NULL);

  if (prepend)
    g_list_store_insert (store, 0, map);
  else
    g_list_store_append (store, map);
}

static void
project_id_changed_cb (GbpShellcmdShortcutProvider *self,
                       GParamSpec                  *pspec,
                       IdeContext                  *context)
{
  g_autoptr(GbpShellcmdCommandModel) project_model = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SHELLCMD_SHORTCUT_PROVIDER (self));
  g_assert (IDE_IS_CONTEXT (context));

  if (g_list_model_get_n_items (G_LIST_MODEL (self->model)) > 1)
    g_list_store_remove (self->model, 0);

  if (ide_context_has_project (context))
    {
      project_model = gbp_shellcmd_command_model_new_for_project (context);
      add_with_mapping (self->model, G_LIST_MODEL (project_model), TRUE);
    }
}

static GListModel *
gbp_shellcmd_shortcut_provider_list_shortcuts (IdeShortcutProvider *provider)
{
  GbpShellcmdShortcutProvider *self = (GbpShellcmdShortcutProvider *)provider;
  GtkFlattenListModel *ret;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SHELLCMD_SHORTCUT_PROVIDER (self));

  if (self->model == NULL)
    {
      IdeContext *context = ide_object_get_context (IDE_OBJECT (self));
      g_autoptr(GbpShellcmdCommandModel) app_model = gbp_shellcmd_command_model_new_for_app ();
      g_autoptr(GbpShellcmdCommandModel) project_model = NULL;

      self->model = g_list_store_new (G_TYPE_LIST_MODEL);

      if (ide_context_has_project (context))
        project_model = gbp_shellcmd_command_model_new_for_project (context);
      else
        g_signal_connect_object (context,
                                 "notify::project-id",
                                 G_CALLBACK (project_id_changed_cb),
                                 self,
                                 G_CONNECT_SWAPPED);

      if (project_model != NULL)
        add_with_mapping (self->model, G_LIST_MODEL (project_model), TRUE);
      add_with_mapping (self->model, G_LIST_MODEL (app_model), FALSE);
    }

  ret = gtk_flatten_list_model_new (g_object_ref (G_LIST_MODEL (self->model)));

  IDE_RETURN (G_LIST_MODEL (ret));
}

static void
shortcut_provider_iface_init (IdeShortcutProviderInterface *iface)
{
  iface->list_shortcuts = gbp_shellcmd_shortcut_provider_list_shortcuts;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpShellcmdShortcutProvider, gbp_shellcmd_shortcut_provider, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_SHORTCUT_PROVIDER, shortcut_provider_iface_init))

static void
gbp_shellcmd_shortcut_provider_destroy (IdeObject *object)
{
  GbpShellcmdShortcutProvider *self = (GbpShellcmdShortcutProvider *)object;

  g_clear_object (&self->model);

  IDE_OBJECT_CLASS (gbp_shellcmd_shortcut_provider_parent_class)->destroy (object);
}

static void
gbp_shellcmd_shortcut_provider_class_init (GbpShellcmdShortcutProviderClass *klass)
{
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  i_object_class->destroy = gbp_shellcmd_shortcut_provider_destroy;
}

static void
gbp_shellcmd_shortcut_provider_init (GbpShellcmdShortcutProvider *self)
{
}
