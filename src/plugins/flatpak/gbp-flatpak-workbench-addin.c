/* gbp-flatpak-workbench-addin.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-flatpak-workbench-addin"

#include <glib/gi18n.h>
#include <libide-io.h>

#include "gbp-flatpak-application-addin.h"
#include "gbp-flatpak-download-stage.h"
#include "gbp-flatpak-workbench-addin.h"

struct _GbpFlatpakWorkbenchAddin
{
  GObject              parent;

  GSimpleActionGroup  *actions;
  IdeWorkbench        *workbench;
  IdeNotification     *message;
};

static void
check_sysdeps_cb (GObject      *object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  GbpFlatpakApplicationAddin *app_addin = (GbpFlatpakApplicationAddin *)object;
  g_autoptr(GbpFlatpakWorkbenchAddin) self = user_data;
  g_autoptr(GError) error = NULL;
  gboolean has_sysdeps;

  g_assert (GBP_IS_FLATPAK_APPLICATION_ADDIN (app_addin));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_FLATPAK_WORKBENCH_ADDIN (self));

  has_sysdeps = gbp_flatpak_application_addin_check_sysdeps_finish (app_addin, result, &error);

#ifdef IDE_ENABLE_TRACE
  if (error != NULL)
    IDE_TRACE_MSG ("which flatpak-builder resulted in %s", error->message);
#endif

  if (!has_sysdeps)
    {
      IdeContext *context;

      context = ide_workbench_get_context (self->workbench);
      ide_notification_attach (self->message, IDE_OBJECT (context));
    }
}

static void
gbp_flatpak_workbench_addin_workspace_added (IdeWorkbenchAddin *addin,
                                             IdeWorkspace      *workspace)
{
  GbpFlatpakWorkbenchAddin *self = (GbpFlatpakWorkbenchAddin *)addin;
  GbpFlatpakApplicationAddin *app_addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_FLATPAK_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  if (!IDE_IS_PRIMARY_WORKSPACE (workspace))
    return;

  gtk_widget_insert_action_group (GTK_WIDGET (workspace), "flatpak",
                                  G_ACTION_GROUP (self->actions));

  self->message = g_object_new (IDE_TYPE_NOTIFICATION,
                                "title", _("Missing system dependencies"),
                                "body", _("The “flatpak-builder” program is necessary for building Flatpak-based applications. Builder can install it for you."),
                                "icon-name", "dialog-warning-symbolic",
                                "urgent", TRUE,
                                NULL);
  ide_notification_add_button (self->message,
                               _("Install"),
                               NULL,
                               "flatpak.install-flatpak-builder");

  app_addin = gbp_flatpak_application_addin_get_default ();
  gbp_flatpak_application_addin_check_sysdeps_async (app_addin,
                                                     NULL,
                                                     check_sysdeps_cb,
                                                     g_object_ref (self));

}

static void
gbp_flatpak_workbench_addin_workspace_removed (IdeWorkbenchAddin *addin,
                                               IdeWorkspace      *workspace)
{
  GbpFlatpakWorkbenchAddin *self = (GbpFlatpakWorkbenchAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_FLATPAK_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  if (!IDE_IS_PRIMARY_WORKSPACE (workspace))
    return;

  gtk_widget_insert_action_group (GTK_WIDGET (workspace), "flatpak", NULL);

  if (self->message != NULL)
    {
      ide_notification_withdraw (self->message);
      g_clear_object (&self->message);
    }
}

static void
gbp_flatpak_workbench_addin_load (IdeWorkbenchAddin *addin,
                                  IdeWorkbench      *workbench)
{
  GbpFlatpakWorkbenchAddin *self = (GbpFlatpakWorkbenchAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_FLATPAK_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = workbench;
}

static void
gbp_flatpak_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                    IdeWorkbench      *workbench)
{
  GbpFlatpakWorkbenchAddin *self = (GbpFlatpakWorkbenchAddin *)addin;

  g_assert (GBP_IS_FLATPAK_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = NULL;
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = gbp_flatpak_workbench_addin_load;
  iface->unload = gbp_flatpak_workbench_addin_unload;
  iface->workspace_added = gbp_flatpak_workbench_addin_workspace_added;
  iface->workspace_removed = gbp_flatpak_workbench_addin_workspace_removed;
}

static void
gbp_flatpak_workbench_addin_install_cb (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  IdeTransferManager *manager = (IdeTransferManager *)object;
  g_autoptr(GbpFlatpakWorkbenchAddin) self = user_data;
  g_autoptr(GError) error = NULL;
  GAction *action;

  IDE_ENTRY;

  g_assert (IDE_IS_TRANSFER_MANAGER (manager));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_FLATPAK_WORKBENCH_ADDIN (self));

  /* Make sure we've not been destroyed */
  if (self->workbench == NULL)
    IDE_EXIT;

  action = g_action_map_lookup_action (G_ACTION_MAP (self->actions), "install-flatpak-builder");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);

  if (!ide_transfer_manager_execute_finish (manager, result, &error))
    {
      /* TODO: Write to message bar */
      g_warning ("Installation of flatpak-builder failed: %s", error->message);
    }
  else
    {
      IdeContext *context = ide_workbench_get_context (self->workbench);
      IdeConfigManager *config_manager = ide_config_manager_from_context (context);

      /* TODO: It would be nice to have a cleaner way to re-setup the pipeline
       *       because we know it is invalidated.
       */
      g_signal_emit_by_name (config_manager, "invalidate");
      ide_notification_withdraw (self->message);
      g_clear_object (&self->message);
    }

  IDE_EXIT;
}

static void
gbp_flatpak_workbench_addin_install_flatpak_builder (GSimpleAction *action,
                                                     GVariant      *param,
                                                     gpointer       user_data)
{
  GbpFlatpakWorkbenchAddin *self = user_data;
  g_autoptr(IdePkconTransfer) transfer = NULL;
  IdeTransferManager *manager;

  static const gchar *packages[] = {
    "flatpak-builder",
    NULL
  };

  IDE_ENTRY;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_FLATPAK_WORKBENCH_ADDIN (self));

  transfer = ide_pkcon_transfer_new (packages);
  manager = ide_transfer_manager_get_default ();

  g_simple_action_set_enabled (action, FALSE);

  ide_transfer_manager_execute_async (manager,
                                      IDE_TRANSFER (transfer),
                                      NULL,
                                      gbp_flatpak_workbench_addin_install_cb,
                                      g_object_ref (self));

  IDE_EXIT;
}

G_DEFINE_TYPE_EXTENDED (GbpFlatpakWorkbenchAddin, gbp_flatpak_workbench_addin, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN, workbench_addin_iface_init))

static void
gbp_flatpak_workbench_addin_finalize (GObject *object)
{
  GbpFlatpakWorkbenchAddin *self = (GbpFlatpakWorkbenchAddin *)object;

  g_clear_object (&self->actions);

  G_OBJECT_CLASS (gbp_flatpak_workbench_addin_parent_class)->finalize (object);
}

static void
gbp_flatpak_workbench_addin_class_init (GbpFlatpakWorkbenchAddinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_flatpak_workbench_addin_finalize;
}

static void
gbp_flatpak_workbench_addin_init (GbpFlatpakWorkbenchAddin *self)
{
  static const GActionEntry actions[] = {
    { "install-flatpak-builder", gbp_flatpak_workbench_addin_install_flatpak_builder },
  };

  self->actions = g_simple_action_group_new ();

  g_action_map_add_action_entries (G_ACTION_MAP (self->actions),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);
}
