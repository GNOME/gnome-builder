/* gbp-update-manager-app-addin.c
 *
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-update-manager-app-addin"

#include "config.h"

#include <libportal/portal.h>
#include <glib/gi18n.h>

#include "gbp-update-manager-app-addin.h"

struct _GbpUpdateManagerAppAddin
{
  GObject          parent_instance;
  IdeApplication  *app;
  XdpPortal       *portal;
  GCancellable    *cancellable;
  IdeNotification *progress_notif;
  IdeNotification *update_notif;
};

static void
on_update_install_cb (GObject      *object,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  XdpPortal *portal = (XdpPortal *)object;
  g_autoptr(GbpUpdateManagerAppAddin) self = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (XDP_IS_PORTAL (portal));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_UPDATE_MANAGER_APP_ADDIN (self));

  if (self->progress_notif)
    {
      ide_notification_withdraw_in_seconds (self->progress_notif, -1);
      g_clear_object (&self->progress_notif);
    }

  if (!xdp_portal_update_install_finish (portal, result, &error))
    {
      g_warning ("Failed to update Builder: %s", error->message);
    }
  else
    {
      if (self->update_notif != NULL)
        {
          ide_notification_withdraw (self->update_notif);
          g_clear_object (&self->update_notif);
        }
    }

  IDE_EXIT;
}

static void
action_update_builder (GSimpleAction *action,
                       GVariant      *param,
                       gpointer       user_data)
{
  GbpUpdateManagerAppAddin *self = user_data;
  IdeWorkbench *workbench;
  IdeContext *context;
  GtkWindow *window;

  IDE_ENTRY;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_UPDATE_MANAGER_APP_ADDIN (self));

  if (self->app == NULL || self->portal == NULL)
    IDE_EXIT;

  if (!(window = gtk_application_get_active_window (GTK_APPLICATION (self->app))))
    IDE_EXIT;

  if (!(workbench = ide_widget_get_workbench (GTK_WIDGET (window))))
    IDE_EXIT;

  context = ide_workbench_get_context (workbench);

  self->progress_notif = ide_notification_new ();
  ide_notification_set_id (self->progress_notif, "org.gnome.builder.update-progress");
  ide_notification_set_icon_name (self->progress_notif, "folder-download-symbolic");
  ide_notification_set_title (self->progress_notif, _("Updating Builder"));
  ide_notification_set_has_progress (self->progress_notif, TRUE);
  ide_notification_attach (self->progress_notif, IDE_OBJECT (context));

  xdp_portal_update_install (self->portal,
                             NULL,
                             XDP_UPDATE_INSTALL_FLAG_NONE,
                             self->cancellable,
                             on_update_install_cb,
                             g_object_ref (self));

  IDE_EXIT;
}

static GActionEntry entries[] = {
  { "update-builder", action_update_builder },
};

static void
on_update_available_cb (GbpUpdateManagerAppAddin *self,
                        const gchar              *current_commit,
                        const gchar              *local_commit,
                        const gchar              *remote_commit,
                        XdpPortal                *portal)
{
  IDE_ENTRY;

  g_assert (GBP_IS_UPDATE_MANAGER_APP_ADDIN (self));
  g_assert (XDP_IS_PORTAL (portal));

  /* The flatpak portal will send an "update available" signal each time a new
   * flatpak push of Builder happens, so it's better to not let notifications pile up.
   */
  if (self->update_notif)
    return;

  if (self->app != NULL)
    {
      IdeWorkbench *workbench;
      IdeContext *context;
      GActionMap *map;
      GtkWindow *window;
      GAction *action;

      map = G_ACTION_MAP (self->app);
      action = g_action_map_lookup_action (map, "update-builder");
      if (G_IS_SIMPLE_ACTION (action))
        g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);

      if (!(window = gtk_application_get_active_window (GTK_APPLICATION (self->app))))
        IDE_EXIT;

      if (!(workbench = ide_widget_get_workbench (GTK_WIDGET (window))))
        IDE_EXIT;

      context = ide_workbench_get_context (workbench);

      self->update_notif = ide_notification_new ();
      ide_notification_set_id (self->update_notif, "org.gnome.builder.update-available");
      ide_notification_set_icon_name (self->update_notif, "software-update-available-symbolic");
      ide_notification_set_title (self->update_notif, _("Update Available"));
      ide_notification_set_body (self->update_notif, _("An update to Builder is available. Builder can download and install it for you."));
      ide_notification_set_urgent (self->update_notif, TRUE);
      ide_notification_add_button (self->update_notif, _("_Update"), NULL, "app.update-builder");
      ide_notification_attach (self->update_notif, IDE_OBJECT (context));
    }

  IDE_EXIT;
}

static void
on_update_progress_cb (GbpUpdateManagerAppAddin *self,
                       guint                     n_ops,
                       guint                     op,
                       guint                     progress,
                       XdpUpdateStatus           status,
                       const gchar              *error,
                       const gchar              *error_message,
                       XdpPortal                *portal)
{
  IDE_ENTRY;

  g_assert (GBP_IS_UPDATE_MANAGER_APP_ADDIN (self));
  g_assert (XDP_IS_PORTAL (portal));

  if (self->progress_notif != NULL)
    ide_notification_set_progress (self->progress_notif, progress / 100.0);

  IDE_EXIT;
}

static void
gbp_update_manager_app_addin_start_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  XdpPortal *portal = (XdpPortal *)object;
  g_autoptr(GbpUpdateManagerAppAddin) self = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (XDP_IS_PORTAL (portal));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_UPDATE_MANAGER_APP_ADDIN (self));

  if (!xdp_portal_update_monitor_start_finish (portal, result, &error))
    g_debug ("Failed to start update monitor: %s", error->message);
  else
    g_message ("Waiting for application updates from libportal");

  IDE_EXIT;
}

static void
gbp_update_manager_app_addin_load (IdeApplicationAddin *addin,
                                   IdeApplication      *app)
{
  GbpUpdateManagerAppAddin *self = (GbpUpdateManagerAppAddin *)addin;
  GAction *action;

  IDE_ENTRY;

  g_assert (GBP_IS_UPDATE_MANAGER_APP_ADDIN (self));
  g_assert (IDE_IS_APPLICATION (app));

  self->app = app;
  self->cancellable = g_cancellable_new ();
  self->portal = xdp_portal_new ();

  g_signal_connect_object (self->portal,
                           "update-available",
                           G_CALLBACK (on_update_available_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->portal,
                           "update-progress",
                           G_CALLBACK (on_update_progress_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_action_map_add_action_entries (G_ACTION_MAP (app),
                                   entries,
                                   G_N_ELEMENTS (entries),
                                   self);

  if ((action = g_action_map_lookup_action (G_ACTION_MAP (app), "update-builder")))
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

  xdp_portal_update_monitor_start (self->portal,
                                   XDP_UPDATE_MONITOR_FLAG_NONE,
                                   self->cancellable,
                                   gbp_update_manager_app_addin_start_cb,
                                   g_object_ref (self));

  IDE_EXIT;
}

static void
gbp_update_manager_app_addin_unload (IdeApplicationAddin *addin,
                                     IdeApplication      *app)
{
  GbpUpdateManagerAppAddin *self = (GbpUpdateManagerAppAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_UPDATE_MANAGER_APP_ADDIN (self));
  g_assert (IDE_IS_APPLICATION (app));

  g_cancellable_cancel (self->cancellable);

  for (guint i = 0; i < G_N_ELEMENTS (entries); i++)
    g_action_map_remove_action (G_ACTION_MAP (app), entries[i].name);

  g_clear_object (&self->portal);
  g_clear_object (&self->cancellable);

  if (self->progress_notif)
    {
      ide_notification_withdraw (self->progress_notif);
      g_clear_object (&self->progress_notif);
    }

  if (self->update_notif)
    {
      ide_notification_withdraw (self->update_notif);
      g_clear_object (&self->update_notif);
    }

  self->app = NULL;

  IDE_EXIT;
}

static void
app_addin_iface_init (IdeApplicationAddinInterface *iface)
{
  iface->load = gbp_update_manager_app_addin_load;
  iface->unload = gbp_update_manager_app_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpUpdateManagerAppAddin, gbp_update_manager_app_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_APPLICATION_ADDIN, app_addin_iface_init))

static void
gbp_update_manager_app_addin_class_init (GbpUpdateManagerAppAddinClass *klass)
{
}

static void
gbp_update_manager_app_addin_init (GbpUpdateManagerAppAddin *self)
{
}
