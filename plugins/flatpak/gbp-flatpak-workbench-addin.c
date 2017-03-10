/* gbp-flatpak-workbench-addin.c
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

#include "gbp-flatpak-workbench-addin.h"

struct _GbpFlatpakWorkbenchAddin
{
  GObject              parent;

  GSimpleActionGroup  *actions;
  IdeWorkbench        *workbench;
  IdeWorkbenchMessage *message;
};

static void
query_packages_cb (GObject      *object,
                   GAsyncResult *result,
                   gpointer      user_data)
{
  GDBusConnection *bus = (GDBusConnection *)object;
  g_autoptr(IdeWorkbenchMessage) message = user_data;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_DBUS_CONNECTION (bus));
  g_assert (G_IS_ASYNC_RESULT (result));

  reply = g_dbus_connection_call_finish (bus, result, &error);

  if (reply != NULL)
    {
      gboolean installed = FALSE;

      g_variant_get (reply, "(b)", &installed);
      gtk_widget_set_visible (GTK_WIDGET (message), !installed);
    }
}

static void
gbp_flatpak_workbench_addin_load (IdeWorkbenchAddin *addin,
                                  IdeWorkbench      *workbench)
{
  GbpFlatpakWorkbenchAddin *self = (GbpFlatpakWorkbenchAddin *)addin;
  g_autoptr(GDBusConnection) bus = NULL;
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

  /*
   * Discover if flatpak-builder is available, and if not, we will show the
   * message bar to the user.
   */
  if (NULL != (bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL)))
    g_dbus_connection_call (bus,
                            "org.freedesktop.PackageKit",
                            "/org/freedesktop/PackageKit",
                            "org.freedesktop.PackageKit.Query",
                            "IsInstalled",
                            g_variant_new ("(ss)", "flatpak-builder", ""),
                            G_VARIANT_TYPE ("(b)"),
                            G_DBUS_CALL_FLAGS_NONE,
                            -1,
                            NULL,
                            query_packages_cb,
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
gbp_flatpak_workbench_addin_update_dependencies (GSimpleAction *action,
                                                 GVariant      *param,
                                                 gpointer       user_data)
{
  GbpFlatpakWorkbenchAddin *self = user_data;
  IdeBuildManager *manager;
  IdeBuildPipeline *pipeline;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_FLATPAK_WORKBENCH_ADDIN (self));

  manager = ide_context_get_build_manager (ide_workbench_get_context (self->workbench));
  pipeline = ide_build_manager_get_pipeline (manager);
  ide_build_pipeline_invalidate_phase (pipeline, IDE_BUILD_PHASE_DOWNLOADS);
  ide_build_manager_execute_async (manager, IDE_BUILD_PHASE_DOWNLOADS, NULL, NULL, NULL);
}

static void
gbp_flatpak_workbench_addin_export_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  IdeBuildManager *manager = (IdeBuildManager *)object;
  g_autoptr(GbpFlatpakWorkbenchAddin) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (GBP_IS_FLATPAK_WORKBENCH_ADDIN (self));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!ide_build_manager_execute_finish (manager, result, &error))
    {
      g_warning ("%s", error->message);
      return;
    }
}

static void
gbp_flatpak_workbench_addin_export (GSimpleAction *action,
                                    GVariant      *param,
                                    gpointer       user_data)
{
  GbpFlatpakWorkbenchAddin *self = user_data;
  IdeBuildManager *manager;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_FLATPAK_WORKBENCH_ADDIN (self));

  manager = ide_context_get_build_manager (ide_workbench_get_context (self->workbench));

  ide_build_manager_execute_async (manager,
                                   IDE_BUILD_PHASE_EXPORT,
                                   NULL,
                                   gbp_flatpak_workbench_addin_export_cb,
                                   g_object_ref (self));
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

  g_assert (IDE_IS_TRANSFER_MANAGER (manager));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_FLATPAK_WORKBENCH_ADDIN (self));

  action = g_action_map_lookup_action (G_ACTION_MAP (self->actions),
                                       "install-flatpak-builder");

  if (!ide_transfer_manager_execute_finish (manager, result, &error))
    /* TODO: Write to message bar */
    g_warning ("%s", error->message);
  else
    gtk_widget_hide (GTK_WIDGET (self->message));

  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);
}

static void
gbp_flatpak_workbench_addin_install_flatpak_builder (GSimpleAction *action,
                                                     GVariant      *param,
                                                     gpointer       user_data)
{
  GbpFlatpakWorkbenchAddin *self = user_data;
  g_autoptr(IdePkconTransfer) transfer = NULL;
  IdeTransferManager *manager;
  IdeContext *context;

  static const gchar *packages[] = {
    "flatpak",
    "flatpak-builder",
    NULL
  };

  IDE_ENTRY;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_FLATPAK_WORKBENCH_ADDIN (self));

  transfer = ide_pkcon_transfer_new (packages);
  context = ide_workbench_get_context (self->workbench);
  manager = ide_context_get_transfer_manager (context);

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
    { "update-dependencies", gbp_flatpak_workbench_addin_update_dependencies },
    { "export", gbp_flatpak_workbench_addin_export },
    { "install-flatpak-builder", gbp_flatpak_workbench_addin_install_flatpak_builder },
  };

  self->actions = g_simple_action_group_new ();

  g_action_map_add_action_entries (G_ACTION_MAP (self->actions),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);
}
