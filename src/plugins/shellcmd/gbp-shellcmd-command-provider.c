/* gbp-shellcmd-command-provider.c
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

#define G_LOG_DOMAIN "gbp-shellcmd-command-provider"

#include "config.h"

#include <dazzle.h>
#include <glib/gi18n.h>
#include <libide-gui.h>
#include <libide-editor.h>
#include <libide-terminal.h>
#include <libide-threading.h>

#include "gbp-shellcmd-application-addin.h"
#include "gbp-shellcmd-command.h"
#include "gbp-shellcmd-command-model.h"
#include "gbp-shellcmd-command-provider.h"

struct _GbpShellcmdCommandProvider
{
  GObject    parent_instance;
  GPtrArray *accels;
  GPtrArray *controllers;
};

static GbpShellcmdCommandModel *
get_model (void)
{
  GbpShellcmdApplicationAddin *app_addin;
  GbpShellcmdCommandModel *model;

  app_addin = ide_application_find_addin_by_module_name (NULL, "shellcmd");
  g_assert (GBP_IS_SHELLCMD_APPLICATION_ADDIN (app_addin));

  model = gbp_shellcmd_application_addin_get_model (app_addin);
  g_assert (GBP_IS_SHELLCMD_COMMAND_MODEL (model));

  return model;
}

static void
gbp_shellcmd_command_provider_query_async (IdeCommandProvider  *provider,
                                           IdeWorkspace        *workspace,
                                           const gchar         *typed_text,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data)
{
  GbpShellcmdCommandProvider *self = (GbpShellcmdCommandProvider *)provider;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GPtrArray) ret = NULL;
  g_autofree gchar *quoted = NULL;
  IdeContext *context;

  g_assert (GBP_IS_SHELLCMD_COMMAND_PROVIDER (provider));
  g_assert (IDE_IS_WORKSPACE (workspace));
  g_assert (typed_text != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_shellcmd_command_provider_query_async);

  ret = g_ptr_array_new_with_free_func ((GDestroyNotify)ide_object_unref_and_destroy);

  gbp_shellcmd_command_model_query (get_model (), ret, typed_text);

  if (!g_shell_parse_argv (typed_text, NULL, NULL, NULL))
    goto skip_commands;

  context = ide_workspace_get_context (workspace);

  g_ptr_array_add (ret,
                   g_object_new (GBP_TYPE_SHELLCMD_COMMAND,
                                 "title", _("Run in host environment"),
                                 "subtitle", typed_text,
                                 "command", typed_text,
                                 "locality", GBP_SHELLCMD_COMMAND_LOCALITY_HOST,
                                 NULL));

  if (ide_context_has_project (context))
    {
      g_ptr_array_add (ret,
                       g_object_new (GBP_TYPE_SHELLCMD_COMMAND,
                                     "title", _("Run in build environment"),
                                     "subtitle", typed_text,
                                     "command", typed_text,
                                     "locality", GBP_SHELLCMD_COMMAND_LOCALITY_BUILD,
                                     NULL));
      g_ptr_array_add (ret,
                       g_object_new (GBP_TYPE_SHELLCMD_COMMAND,
                                     "title", _("Run in runtime environment"),
                                     "subtitle", typed_text,
                                     "command", typed_text,
                                     "locality", GBP_SHELLCMD_COMMAND_LOCALITY_RUN,
                                     NULL));
    }

skip_commands:
  ide_task_return_pointer (task,
                           g_steal_pointer (&ret),
                           g_ptr_array_unref);
}

static GPtrArray *
gbp_shellcmd_command_provider_query_finish (IdeCommandProvider  *provider,
                                            GAsyncResult        *result,
                                            GError             **error)
{
  g_autoptr(GPtrArray) ret = NULL;

  g_assert (GBP_IS_SHELLCMD_COMMAND_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);
  return IDE_PTR_ARRAY_STEAL_FULL (&ret);
}

static void
remove_shortcuts (GbpShellcmdCommandProvider *self,
                  DzlShortcutController      *controller)
{
  g_assert (GBP_IS_SHELLCMD_COMMAND_PROVIDER (self));
  g_assert (DZL_IS_SHORTCUT_CONTROLLER (controller));

  for (guint i = 0; i < self->accels->len; i++)
    {
      const gchar *accel = g_ptr_array_index (self->accels, i);

      dzl_shortcut_controller_remove_accel (controller,
                                            accel,
                                            DZL_SHORTCUT_PHASE_CAPTURE | DZL_SHORTCUT_PHASE_GLOBAL);
    }
}

static void
on_model_keybindings_changed_cb (GbpShellcmdCommandProvider *self,
                                 GbpShellcmdCommandModel    *model)
{
  guint n_items;

  g_assert (GBP_IS_SHELLCMD_COMMAND_PROVIDER (self));
  g_assert (GBP_IS_SHELLCMD_COMMAND_MODEL (model));

  for (guint j = 0; j < self->controllers->len; j++)
    {
      DzlShortcutController *controller = g_ptr_array_index (self->controllers, j);

      remove_shortcuts (self, controller);
    }

  if (self->accels->len > 0)
    g_ptr_array_remove_range (self->accels, 0, self->accels->len);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (model));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(GbpShellcmdCommand) command = NULL;
      g_autofree gchar *dzlcmdid = NULL;
      g_autofree gchar *dzlcmdaction = NULL;
      g_autofree gchar *delimit = NULL;
      const gchar *shortcut;
      const gchar *id;

      command = g_list_model_get_item (G_LIST_MODEL (model), i);
      id = gbp_shellcmd_command_get_id (command);
      shortcut = gbp_shellcmd_command_get_shortcut (command);

      if (id == NULL || shortcut == NULL || shortcut[0] == 0)
        continue;

      dzlcmdid = g_strdup_printf ("org.gnome.builder.plugins.shellcmd.%s", id);
      dzlcmdaction = g_strdup_printf ("win.command('%s')", id);

      for (guint j = 0; j < self->controllers->len; j++)
        {
          DzlShortcutController *controller = g_ptr_array_index (self->controllers, j);

          dzl_shortcut_controller_add_command_action (controller,
                                                      dzlcmdid,
                                                      shortcut,
                                                      DZL_SHORTCUT_PHASE_CAPTURE | DZL_SHORTCUT_PHASE_GLOBAL,
                                                      dzlcmdaction);
        }

      g_ptr_array_add (self->accels, g_strdup (shortcut));
    }
}

static void
gbp_shellcmd_command_provider_unload_shortcuts (IdeCommandProvider *provider,
                                                IdeWorkspace       *workspace)
{
  GbpShellcmdCommandProvider *self = (GbpShellcmdCommandProvider *)provider;
  DzlShortcutController *controller;

  g_return_if_fail (GBP_IS_SHELLCMD_COMMAND_PROVIDER (self));
  g_return_if_fail (IDE_IS_WORKSPACE (workspace));

  if ((controller = dzl_shortcut_controller_try_find (GTK_WIDGET (workspace))))
    {
      remove_shortcuts (self, controller);
      g_ptr_array_remove (self->controllers, controller);
    }
}

static void
gbp_shellcmd_command_provider_load_shortcuts (IdeCommandProvider *provider,
                                              IdeWorkspace       *workspace)
{
  GbpShellcmdCommandProvider *self = (GbpShellcmdCommandProvider *)provider;
  DzlShortcutController *controller;

  g_return_if_fail (GBP_IS_SHELLCMD_COMMAND_PROVIDER (self));
  g_return_if_fail (IDE_IS_WORKSPACE (workspace));

  /* Limit ourselves to some known workspaces */
  if (!IDE_IS_PRIMARY_WORKSPACE (workspace) &&
      !IDE_IS_EDITOR_WORKSPACE (workspace) &&
      !IDE_IS_TERMINAL_WORKSPACE (workspace))
    return;

  controller = dzl_shortcut_controller_find (GTK_WIDGET (workspace));

  g_assert (dzl_shortcut_controller_get_widget (controller) == GTK_WIDGET (workspace));

  g_ptr_array_add (self->controllers, g_object_ref (controller));

  on_model_keybindings_changed_cb (self, get_model ());
}

static IdeCommand *
gbp_shellcmd_command_provider_get_command_by_id (IdeCommandProvider *provider,
                                                 IdeWorkspace       *workspace,
                                                 const gchar        *command_id)
{
  GbpShellcmdCommand *command;

  g_return_val_if_fail (GBP_IS_SHELLCMD_COMMAND_PROVIDER (provider), NULL);
  g_return_val_if_fail (IDE_IS_WORKSPACE (workspace), NULL);
  g_return_val_if_fail (command_id != NULL, NULL);

  if ((command = gbp_shellcmd_command_model_get_command (get_model (), command_id)))
    return IDE_COMMAND (gbp_shellcmd_command_copy (command));

  return NULL;
}

static void
command_provider_iface_init (IdeCommandProviderInterface *iface)
{
  iface->query_async = gbp_shellcmd_command_provider_query_async;
  iface->query_finish = gbp_shellcmd_command_provider_query_finish;
  iface->load_shortcuts = gbp_shellcmd_command_provider_load_shortcuts;
  iface->unload_shortcuts = gbp_shellcmd_command_provider_unload_shortcuts;
  iface->get_command_by_id = gbp_shellcmd_command_provider_get_command_by_id;
}

G_DEFINE_TYPE_WITH_CODE (GbpShellcmdCommandProvider, gbp_shellcmd_command_provider, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_COMMAND_PROVIDER,
                                                command_provider_iface_init))

static void
gbp_shellcmd_command_provider_finalize (GObject *object)
{
  GbpShellcmdCommandProvider *self = (GbpShellcmdCommandProvider *)object;

  g_clear_pointer (&self->accels, g_ptr_array_unref);
  g_clear_pointer (&self->controllers, g_ptr_array_unref);

  G_OBJECT_CLASS (gbp_shellcmd_command_provider_parent_class)->finalize (object);
}

static void
gbp_shellcmd_command_provider_class_init (GbpShellcmdCommandProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_shellcmd_command_provider_finalize;
}

static void
gbp_shellcmd_command_provider_init (GbpShellcmdCommandProvider *self)
{
  self->accels = g_ptr_array_new_with_free_func (g_free);
  self->controllers = g_ptr_array_new_with_free_func (g_object_unref);

  g_signal_connect_object (get_model (),
                           "keybindings-changed",
                           G_CALLBACK (on_model_keybindings_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
}
