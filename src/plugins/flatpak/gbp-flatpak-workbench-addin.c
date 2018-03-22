/* gbp-flatpak-workbench-addin.c
 *
 * Copyright 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-flatpak-workbench-addin"

#include <glib/gi18n.h>

#include "gbp-flatpak-application-addin.h"
#include "gbp-flatpak-download-stage.h"
#include "gbp-flatpak-workbench-addin.h"

struct _GbpFlatpakWorkbenchAddin
{
  GObject              parent;

  GSimpleActionGroup  *actions;
  IdeWorkbench        *workbench;
  IdeWorkbenchMessage *message;
};

static void
check_sysdeps_cb (GObject      *object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  GbpFlatpakApplicationAddin *app_addin = (GbpFlatpakApplicationAddin *)object;
  g_autoptr(IdeWorkbenchMessage) message = user_data;
  g_autoptr(GError) error = NULL;
  gboolean has_sysdeps;

  g_assert (GBP_IS_FLATPAK_APPLICATION_ADDIN (app_addin));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_WORKBENCH_MESSAGE (message));

  has_sysdeps = gbp_flatpak_application_addin_check_sysdeps_finish (app_addin, result, &error);

#ifdef IDE_ENABLE_TRACE
  if (error != NULL)
    IDE_TRACE_MSG ("which flatpak-builder resulted in %s", error->message);
#endif

  gtk_widget_set_visible (GTK_WIDGET (message), has_sysdeps == FALSE);
}

static void
gbp_flatpak_workbench_addin_load (IdeWorkbenchAddin *addin,
                                  IdeWorkbench      *workbench)
{
  GbpFlatpakWorkbenchAddin *self = (GbpFlatpakWorkbenchAddin *)addin;
  GbpFlatpakApplicationAddin *app_addin;
  IdeContext *context;

  g_assert (GBP_IS_FLATPAK_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = workbench;

  context = ide_workbench_get_context (workbench);
  if (context != NULL)
    gtk_widget_insert_action_group (GTK_WIDGET (workbench), "flatpak", G_ACTION_GROUP (self->actions));

  self->message = g_object_new (IDE_TYPE_WORKBENCH_MESSAGE,
                                "id", "org.gnome.builder.flatpak.install",
                                "title", _("Your computer is missing flatpak-builder"),
                                "subtitle", _("This program is necessary for building Flatpak applications. Would you like to install it?"),
                                "show-close-button", TRUE,
                                "visible", FALSE,
                                NULL);
  ide_workbench_message_add_action (self->message, _("Install"), "flatpak.install-flatpak-builder");
  ide_workbench_push_message (workbench, self->message);

  app_addin = gbp_flatpak_application_addin_get_default ();
  gbp_flatpak_application_addin_check_sysdeps_async (app_addin,
                                                     NULL,
                                                     check_sysdeps_cb,
                                                     g_object_ref (self->message));
}

static void
gbp_flatpak_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                    IdeWorkbench      *workbench)
{
  GbpFlatpakWorkbenchAddin *self = (GbpFlatpakWorkbenchAddin *)addin;

  g_assert (GBP_IS_FLATPAK_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  gtk_widget_insert_action_group (GTK_WIDGET (workbench), "flatpak", NULL);

  gtk_widget_destroy (GTK_WIDGET (self->message));

  self->message = NULL;
  self->workbench = NULL;
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = gbp_flatpak_workbench_addin_load;
  iface->unload = gbp_flatpak_workbench_addin_unload;
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
      IdeConfigurationManager *config_manager = ide_context_get_configuration_manager (context);

      /* TODO: It would be nice to have a cleaner way to re-setup the pipeline
       *       because we know it is invalidated.
       */
      g_signal_emit_by_name (config_manager, "invalidate");
      gtk_widget_hide (GTK_WIDGET (self->message));
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
  manager = ide_application_get_transfer_manager (IDE_APPLICATION_DEFAULT);

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
